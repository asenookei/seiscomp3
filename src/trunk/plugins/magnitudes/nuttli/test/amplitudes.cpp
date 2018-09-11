/***************************************************************************
 *   Copyright (C) by gempa GmbH                                           *
 *                                                                         *
 *   You can redistribute and/or modify this program under the             *
 *   terms of the SeisComP Public License.                                 *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   SeisComP Public License for more details.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT TestAmplitudes

#include <seiscomp3/logging/log.h>
#include <seiscomp3/client/application.h>
#include <seiscomp3/client/inventory.h>
#include <seiscomp3/datamodel/eventparameters_package.h>
#include <seiscomp3/utils/misc.h>
#include <seiscomp3/processing/amplitudeprocessor.h>
#include <seiscomp3/processing/magnitudeprocessor.h>
#include <seiscomp3/io/archive/xmlarchive.h>
#include <seiscomp3/io/recordstream/file.h>

#include <boost/bind.hpp>
#include <map>
#include <iostream>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;


#define AMP_TYPE "AMN"
#define MAG_TYPE "MN"


class TestApp : public Client::Application {
	public:
		TestApp(int argc, char** argv)
		: Client::Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(true, true);
			setLoadInventoryEnabled(true);
			setLoggingToStdErr(true);
		}

	public:
		virtual bool initConfiguration() {
			if ( !Client::Application::initConfiguration() )
				return false;
			setDatabaseEnabled(false, false);

			_configuration.setBool("module.trunk.global.amplitudes." AMP_TYPE ".enableResponses", true);

			return true;
		}

		bool compareAmplitudes(const char *prefix) {
			AmplitudeProcMap amplitudeProcPickRoute;
			AmplitudeProcMap amplitudeProcStreamRoute;

			SEISCOMP_NOTICE("%s", prefix);
			SEISCOMP_NOTICE("=================================================");

			IO::XMLArchive ar;
			if ( !ar.open((string("data/") + prefix + ".xml").c_str()) ) {
				cerr << "Failed to open event XML: " << prefix << ".xml" << endl;
				return false;
			}

			EventParametersPtr ep;
			ar >> ep;
			if ( !ep ) {
				cerr << "No event parameters found in " << prefix << ".xml" << endl;
				return false;
			}

			if ( ep->originCount() == 0 ) {
				cerr << "Expectation failed: no origin found: " << prefix << ".xml" << endl;
				return false;
			}

			if ( ep->originCount() > 1 ) {
				cerr << "Expectation failed: more than 1 origin found: " << prefix << ".xml" << endl;
				return false;
			}

			OriginPtr origin = ep->origin(0);
			if ( origin->arrivalCount() == 0 ) {
				cerr << "Expectation failed: no arrivals for origin found: " << prefix << ".xml" << endl;
				return false;
			}

			EventPtr evt =  ep->eventCount() > 0 ? ep->event(0) : NULL;

			AmplitudeMap originalAmplitudes;

			// Load existing amplitudes and create a map from pickID to the
			// amplitude object.
			for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
				AmplitudePtr amp = ep->amplitude(i);
				// Just ignore non MN amplitudes
				if ( amp->type() != AMP_TYPE ) continue;

				if ( amp->pickID().empty() ) {
					cerr << "Expectation failed: amplitude '" << amp->publicID() << "' without pick reference: " << prefix << ".xml" << endl;
					return false;
				}

				originalAmplitudes[amp->pickID()] = pair<AmplitudePtr, string>(amp, "");
			}

			DataModel::Inventory *inv = Client::Inventory::Instance()->inventory();

			// Load data and forward them to the respective amplitude processors
			RecordStream::File file(string("data/") + prefix + ".mseed");

			int i = 0;
			for ( AmplitudeMap::iterator it = originalAmplitudes.begin();
			      it != originalAmplitudes.end(); ++it, ++i ) {
				Pick *pick = Pick::Find(it->second.first->pickID());
				if ( pick == NULL ) {
					SEISCOMP_WARNING("Ignoring amplitude %d: pick not found",
					                 int(i));
					continue;
				}

				DataModel::SensorLocation *loc = DataModel::getSensorLocation(inv, pick);
				if ( loc == NULL ) {
					SEISCOMP_WARNING("Ignoring amplitude %d: %s.%s.%s: sensor location not found", int(i),
					                 pick->waveformID().networkCode().c_str(),
					                 pick->waveformID().stationCode().c_str(),
					                 pick->waveformID().locationCode().c_str());
					continue;
				}

				DataModel::Stream *chan = DataModel::getVerticalComponent(
					loc, pick->waveformID().channelCode().substr(0,2).c_str(),
					pick->time().value()
				);

				if ( chan == NULL ) {
					SEISCOMP_WARNING("Ignoring amplitude %d: vertical channel for %s.%s.%s not found",
					                 int(i),
					                 pick->waveformID().networkCode().c_str(),
					                 pick->waveformID().stationCode().c_str(),
					                 pick->waveformID().locationCode().c_str());
					continue;
				}

				string sid = pick->waveformID().networkCode() + "." +
				             pick->waveformID().stationCode() + "." +
				             loc->code() + "." + chan->code();

				if ( amplitudeProcStreamRoute.find(sid) != amplitudeProcStreamRoute.end() )
					// Same stream id already used
					continue;

				// Inject sid into existing

				Processing::Settings settings(configModuleName(),
				                              pick->waveformID().networkCode(),
				                              pick->waveformID().stationCode(),
				                              pick->waveformID().locationCode(),
				                              chan->code(),
				                              &configuration(), NULL);

				Processing::AmplitudeProcessorPtr proc;
				proc = Processing::AmplitudeProcessorFactory::Create(AMP_TYPE);

				proc->streamConfig(Processing::WaveformProcessor::VerticalComponent).init(chan);
				proc->setTrigger(pick->time().value());
				proc->setPick(pick);

				if ( !proc->setup(settings) ) {
					SEISCOMP_WARNING("Ignoring amplitude %d: processor failed to setup: %s (%f)",
					                 int(i),
					                 proc->status().toString(),
					                 proc->statusValue());
					continue;
				}

				proc->setEnvironment(origin.get(), loc, pick);
				proc->computeTimeWindow();

				if ( proc->isFinished() ) {
					SEISCOMP_WARNING("Ignoring amplitude %d: processor has already finished: %s (%f)",
					                 int(i),
					                 proc->status().toString(),
					                 proc->statusValue());
					continue;
				}

				proc->setPublishFunction(boost::bind(&TestApp::publishAmplitude, this, _1, _2));
				amplitudeProcStreamRoute[sid] = proc;
				amplitudeProcPickRoute[it->second.first->pickID()] = proc;
				it->second.second = sid;

				file.addStream(pick->waveformID().networkCode(),
				               pick->waveformID().stationCode(),
				               loc->code(), chan->code(),
				               proc->safetyTimeWindow().startTime(),
				               proc->safetyTimeWindow().endTime());
			}

			if ( amplitudeProcStreamRoute.empty() ) {
				cerr << "No processors created" << endl;
				return false;
			}

			// Read and forward data
			RecordPtr rec;
			while ( (rec = file.next()) ) {
				AmplitudeProcMap::iterator it = amplitudeProcStreamRoute.find(rec->streamID());
				if ( it == amplitudeProcStreamRoute.end() ) continue;
				if ( !it->second->isFinished() )
					it->second->feed(rec.get());
			}

			// Analyse results
			for ( AmplitudeMap::iterator it = originalAmplitudes.begin();
			      it != originalAmplitudes.end(); ++it ) {

				if ( it->second.second.empty() ) continue;

				cerr << "[" << it->second.second << "]" << endl
				     << "  - " << it->first << endl
				     << "    - amp          = " << it->second.first->amplitude().value() << endl
				     << "    - SNR          = ";
				try {
					cerr << it->second.first->snr();
				}
				catch ( ... ) {
					cerr << "<unset>";
				}
				cerr << endl
				     << "    - period       = ";
				try {
					cerr << it->second.first->period().value();
				}
				catch ( ... ) {
					cerr << "<unset>";
				}
				cerr << endl
				     << "    - time of peak = ";
				try {
					cerr << it->second.first->timeWindow().reference().iso();
				}
				catch ( ... ) {
					cerr << "<unset>";
				}
				cerr << endl;
				cerr << endl
				     << "    - time of peak = ";
				try {
					cerr << it->second.first->timeWindow().reference().iso();
				}
				catch ( ... ) {
					cerr << "<unset>";
				}
				cerr << endl;

				// Find corresponding reprocessed amplitude
				AmplitudeProcMap::iterator ait = amplitudeProcPickRoute.find(it->first);
				if ( ait == amplitudeProcPickRoute.end() ) {
					cerr << "  - nothing reprocessed" << endl;
					continue;
				}

				if ( ait->second->userData() != NULL ) {
					Amplitude *amp = static_cast<Amplitude*>(ait->second->userData());
					cerr << "  + " << amp->publicID() << endl
					     << "    + amp          = " << amp->amplitude().value() << endl
					     << "    + SNR          = " << amp->snr() << endl
					     << "    + period       = " << amp->period().value() << endl
					     << "    + time of peak = " << amp->timeWindow().reference().iso() << endl;
				}
				else
					cerr << "  ! processing failed" << endl
					     << "    + error =  " << ait->second->status().toString() << endl
					     << "    + error value = " << ait->second->statusValue() << endl;
			}

			cerr << "# channel; amplitude in m/s; snr; period; time of peak; rel tw begin; rel tw end" << endl;
			for ( AmplitudeMap::iterator it = originalAmplitudes.begin();
			      it != originalAmplitudes.end(); ++it ) {
				AmplitudeProcMap::iterator ait = amplitudeProcPickRoute.find(it->first);
				if ( ait == amplitudeProcPickRoute.end() ) continue;

				if ( ait->second->userData() != NULL ) {
					Amplitude *amp = static_cast<Amplitude*>(ait->second->userData());
					cerr << it->second.second << ";"
					     << amp->amplitude().value() << ";"
					     << amp->snr() << ";"
					     << amp->period().value() << ";"
					     << amp->timeWindow().reference().iso() << ";"
					     << amp->timeWindow().begin() << ";"
					     << amp->timeWindow().end()
					     << endl;
				}
			}

			// Clear old magnitudes
			while ( origin->magnitudeCount() > 0 )
				origin->removeMagnitude(0);
			while ( origin->stationMagnitudeCount() > 0 )
				origin->removeStationMagnitude(0);

			Processing::MagnitudeProcessorPtr magProc = Processing::MagnitudeProcessorFactory::Create(MAG_TYPE);
			Core::Time now = Core::Time::GMT();

			ep = NULL;
			ep = new EventParameters;
			for ( AmplitudeProcMap::iterator it = amplitudeProcPickRoute.begin();
			      it != amplitudeProcPickRoute.end(); ++it ) {
				if ( it->second->userData() == NULL ) continue;
				Amplitude *amp = static_cast<Amplitude*>(it->second->userData());
				ep->add(amp);

				Processing::Settings settings(configModuleName(),
				                              amp->waveformID().networkCode(),
				                              amp->waveformID().stationCode(),
				                              amp->waveformID().locationCode(),
				                              amp->waveformID().channelCode(),
				                              &configuration(), NULL);

				if ( ! magProc->setup(settings)) {
					SEISCOMP_ERROR("magnitude processor failed to setup");
					return false;
				}

				const Processing::AmplitudeProcessor::Environment &env = it->second->environment();

				double mag;
				Processing::MagnitudeProcessor::Status status =
					magProc->computeMagnitude(amp->amplitude().value(), amp->unit(),
					                          amp->period().value(), amp->snr(),
					                          -1, -1, // Not required
					                          env.hypocenter, env.receiver, amp, mag);

				if ( status != Processing::MagnitudeProcessor::OK ) {
					SEISCOMP_WARNING("%s.%s.%s.%s: magnitude status = %s",
					                 amp->waveformID().networkCode().c_str(),
					                 amp->waveformID().stationCode().c_str(),
					                 amp->waveformID().locationCode().c_str(),
					                 amp->waveformID().channelCode().c_str(),
					                 status.toString());
					continue;
				}

				StationMagnitudePtr stamag = StationMagnitude::Create();

				CreationInfo ci;
				ci.setAgencyID(agencyID());
				ci.setAuthor(author());
				ci.setCreationTime(now);
				stamag->setCreationInfo(ci);

				stamag->setType(magProc->type());
				stamag->setWaveformID(amp->waveformID());
				stamag->setAmplitudeID(amp->publicID());
				stamag->setOriginID(origin->publicID());

				stamag->setMagnitude(mag);
				magProc->finalizeMagnitude(stamag.get());

				origin->add(stamag.get());
			}

			ep->add(origin.get());

			if ( !evt )
				evt = Event::Create();

			evt->setPreferredOriginID(origin->publicID());
			evt->setPreferredMagnitudeID("");
			evt->setPreferredFocalMechanismID("");

			while ( evt->originReferenceCount() > 0 )
				evt->removeOriginReference(0);

			evt->add(new OriginReference(origin->publicID()));

			ep->add(evt.get());

			SEISCOMP_DEBUG("Output result to " TEST_BUILD_DIR "/%s_test.xml", prefix);
			ar.create((string(TEST_BUILD_DIR) + "/" + prefix + "_test.xml").c_str());
			ar.setFormattedOutput(true);
			ar << ep;
			ar.close();

			return true;
		}

		void publishAmplitude(const Processing::AmplitudeProcessor *proc,
		                      const Processing::AmplitudeProcessor::Result &res) {
			AmplitudePtr amp = Amplitude::Create();
			CreationInfo ci;
			if ( amp == NULL ) return;

			amp->setAmplitude(
				RealQuantity(
					res.amplitude.value, Core::None,
					res.amplitude.lowerUncertainty, res.amplitude.upperUncertainty,
					Core::None
				)
			);

			if ( res.period > 0 ) amp->setPeriod(RealQuantity(res.period));
			if ( res.snr >= 0 ) amp->setSnr(res.snr);
			amp->setType(proc->type());
			amp->setUnit(proc->unit());
			amp->setTimeWindow(
				DataModel::TimeWindow(res.time.reference, res.time.begin, res.time.end)
			);

			if ( res.component <= Processing::WaveformProcessor::SecondHorizontal )
				amp->setWaveformID(
					WaveformStreamID(
						res.record->networkCode(), res.record->stationCode(),
						res.record->locationCode(), proc->streamConfig((Processing::WaveformProcessor::Component)res.component).code(), ""
					)
				);
			else
				amp->setWaveformID(
					WaveformStreamID(
						res.record->networkCode(), res.record->stationCode(),
						res.record->locationCode(), res.record->channelCode().substr(0,2), ""
					)
				);

			amp->setPickID(proc->referencingPickID());

			Core::Time now = Core::Time::GMT();
			ci.setAgencyID(agencyID());
			ci.setAuthor(author());
			ci.setCreationTime(now);
			amp->setCreationInfo(ci);

			proc->finalizeAmplitude(amp.get());
			proc->setUserData(amp.get());
		}

		virtual bool run() {
			// The location of the region definition file must be changed because
			// the software and its data files is probably not yet installed.
			// Tests must be executable in the build environment.
			_configuration.setString("magnitudes.MN.region", "../data/MN.bna");

			{
				Processing::AmplitudeProcessorPtr proc = Processing::AmplitudeProcessorFactory::Create(AMP_TYPE);
				if ( !proc ) {
					cerr << "Failed to create amplitude processor for type MN" << endl;
					return false;
				}
			}

			{
				Processing::MagnitudeProcessorPtr proc = Processing::MagnitudeProcessorFactory::Create(MAG_TYPE);
				if ( !proc ) {
					cerr << "Failed to create magnitude processor for type MN" << endl;
					return false;
				}
			}

			if ( !compareAmplitudes("BarrowStrait_MN6.0") )
				return false;

			if ( !compareAmplitudes("Charlevoix_MN2.4") )
				return false;

			return true;
		}

	private:
		typedef map<string, pair<AmplitudePtr, string> > AmplitudeMap;
		typedef map<string, Processing::AmplitudeProcessorPtr> AmplitudeProcMap;
};


int main(int argc, char **argv) {
	TestApp test(argc, argv);
	return test();
}
