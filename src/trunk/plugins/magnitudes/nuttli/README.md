# Geological Survey of Canada Nuttli magnitude implementation notes
Nick Ackerley and Michal Kolaj, Canadian Hazards Information Service

## Contents

1.	Introduction
2.	Overview
 1.	Basic Formula
 2.	Quality Control
3.	Current Implementation
 1.	Instrument response correction
 2.	Amplitude and period estimation
4.	Implementation Enhancements
 1.	Windowing
 2.	Signal-to-noise ratio estimation
 3.	Region checking
 4.	Filtering
 5.	Instrument response correction
 6.	Aggregation
5.	Result and metadata storage
 1.	Amplitude
 2.	StationMagnitude
 3.	Magnitude
 4.	StationMagnitudeContributions
 5.	CreationInfo
6.	Regression Testing
7.	References
8.  Appendix A: Peak detection algorithm

## 1. Introduction

This document describes the current procedure for computing Geological Survey of
Canada Nuttli magnitude (MN) in detail. It is intended to serve to record the
current practice and as the basis for future implementations on different
software platforms.

The Nuttli magnitude MN has been in use in eastern Canada since the 1970s.
This method of estimating magnitude depends on the picking of amplitudes in the
Lg phase, a superposition of waves internally reflected within the crust. This
phase is particularly well developed in stable cratons such as eastern North
America. The basis of the method is described by Nuttli [CITATION Nut73 \n  \t  \l 4105 ].

A significant modification of the procedure, documented only in an unpublished
abstract [ CITATION Wet82 \l 4105 ] was adopted upon implementation in eastern
Canada. Wetmiller & Drysdale showed that the scaling relation for 4° < ∆ < 30°
worked better than that originally proposed for 0.5° < ∆ < 4° over the entire
picentral distance range. The program currently used for estimating peak
velocity from digital seismograms is called DAN [ CITATION Nan97 \l 4105 ].

The process for computing MN has remained unchanged since at least 1990s,
although changes in instrumentation may have affected the results
obtained [ CITATION Ben14 \l 4105 ].

## 2. Overview
### 2.1 Basic Formula

Station magnitudes are calculated from trace amplitudes and epicentral distances as follows:

```
MN = 3.3 + 1.66*log10(delta) + log10(V/(2*PI))
```

Where ∆ is the epicentral distance in degrees and  is half the maximum peak-to-peak amplitude in um/s measured on the vertical instrument-corrected velocity trace.  The window in which  is measured must contain the entire Lg wave group. The period  of the peak ground motion is estimated, and though it is not part of the  formula, it is used to check the validity of the result.
Network magnitudes are calculated by aggregating station magnitudes. This is typically done by taking the mean.

### 2.2 Quality Control

A station magnitude is only valid if the following conditions are met:

* Period: 0.01 s < T < 1.3 s
* Distance: 0.5° < ∆ < 30°
* Signal-to-noise ratio: SNR > 2
* Path from source to receiver is contained within eastern North America

## 3. Current Implementation

In the current implementation, response correction, peak detection, amplitude
and period estimation, period validation and station magnitude computation are
handled automatically. Window selection, distance validation and the remaining
quality control is done manually by the analyst. Network magnitudes are computed
by taking the mean of station magnitudes; the analyst is responsible for
removing invalid measurements and other outliers by manually marking station
magnitudes that should be omitted from the mean.

### 3.1 Instrument response correction

In the current implementation, only the instrument sensitivity is corrected
prior to amplitude and period estimation. The amplitude is then adjusted by a
period-dependent correction factor derived from the instrument poles and zeros.
This must be the default behaviour of any future implementation, until it can
be demonstrated that another method produces equivalent results more robustly.

### 3.2 Amplitude and period estimation

The peak-to-peak amplitude is be calculated by searching the waveform
sequentially within a predefined window. Peaks are identified by detecting
changes of sign in the first difference of the waveform. Periods estimated as
double the time from peak to peak. Although there may be ways to more robustly
estimate the peak amplitude we are constrained by the requirement to emulate the
behavior of legacy systems.
See Appendix A for an implementation in Perl of the required algorithm.

## 4. Implementation Enhancements

In future implementations, it is expected that window selection and the
validation of distance range, region and signal-to-noise ratio can all be
automated. Furthermore, some optional improvements relating to filtering,
response correction and station magnitude aggregation are proposed.

### 4.1 Windowing

The aim of windowing is to select the time segment most likely to contain the
peak Lg arrival(s). Legacy systems relied on manual selection of windows; we are
free to optimize an automatic windowing system but the system must permit a
manual override.
When manually picked phases are available, they should constrain the start and
end of the window; otherwise phase arrival times should be calculated from the
locally applicable velocity model. Start and end of the window should be based
on a priority system:

* signal_window_start = first(Lg, Sg, Sn, S, Vmax)
* signal_window_end = first(Rg, Vmin)

The priority list, Vmax and Vmin should be configurable. By default
Vmax = 3.6 km/s and Vmin = 3.2 km/s.

Window start and end times must be adjusted earlier and later, respectively,
by the uncertainties  assigned to the picks associated with the relevant
arrivals. If the arrivals are not available or if the picks have not been
assigned an uncertainty then a default uncertainty should be applied:

Where  is the epicentral distance in km and  is the s-wave propagation velocity
in the upper crust.

### 4.2 Signal-to-noise ratio estimation

A noise window is selected which has the same duration as the signal window but
which starts a configurable number of seconds before a start phase:

* noise_window_end = first(Pg, Pn, P) – noise_window_pre_seconds
* noise_window_start = noise_window_end - signal_window_end + signal_window_start

As with the signal window, manually picked phases are used if available,
otherwise the locally applicable velocity model is used.
By default, the noise amplitude is estimated in the same way as the signal
amplitude. As a configurable option, both signal and noise can be estimated as
the RMS over the relevant window.

### 4.3 Region checking

A polygon of latitude and longitude coordinates representing eastern North
America shall be defined which has as its western limit the foothills of the
western cordillera, while to the north, east and south it extends as far as the
continental shelf. The polygon need not be configurable.
It may be acceptable to simplify region checking by ensuring only that both the
source and receiver are within the polygon, but strictly speaking, it is
preferable to ensure that the entire path from source to receiver lies entirely
within the polygon.

### 4.4 Filtering

The plugin must support a configurable filter such that the V measurement is
made on a filtered trace. By default, filtering is not enabled.

### 4.5 Instrument response correction

A problem with the current form of response correction is that instrument
response can influence peak detection. Seismometers do not all have perfectly
flat responses in the band of interest.

It is proposed that in future implementations of MN, there should be configurable
option to correct the full instrument response prior to peak detection. It will
be necessary to apply a high pass filter with a corner frequency well below the
frequency range of interest in order to ensure the output is bounded after
response removal; a corner period of 100 s is proposed. Full response removal
eliminates the need to correct amplitudes afterwards.

### 4.6 Aggregation

Network magnitudes must support estimation from station magnitudes via the mean,
trimmed mean or median. By default, the network magnitude is the mean of the
station magnitudes.

### 5. Result and metadata storage

This section documents the required result and metadata storage for future
implementations. A notable defect of current implementations is that station and
network magnitude values are rounded too aggressively, to one decimal place.

The following data resulting from each magnitude estimation shall be stored.
Note that the attribute naming convention followed here is that of
QuakeML [ CITATION Sch11 \l 4105 ].

Station magnitudes that are rejected due to low signal-to-noise or period should
be recorded, even if they are not used.

### 5.1 Amplitude

```
generic_amplitude =  [m/s] rounded to 5 significant figures
generic_amplitude_errors = None (until and unless we develop an appropriate estimator)
type = “AMN”
category = “point”
unit = “m/s”
method_id = resource identifier documenting any non-default setup other than filtering, e.g. non-default window start/end phases, response correction or SNR estimation
period =  [s] rounded to 3 significant figures
snr = estimated signal-to-noise ratio (see Section 4.2) rounded to 3 significant figures
time_window
reference = sample time of first peak value [UTC]
begin = time elapsed from first sample of window to reference [s]
end = time elapsed from reference to last sample of window [s]
pick_id = id of pick used to constrain window start, if any, else None
waveform_id = id of trace on which amplitude was measured
filter_id = description of optional filter used, if any, else None
magnitude_hint = “MN”
evaluation_mode = “manual” if window start or end were manually adjusted else “automatic”
evaluation_status = “preliminary”
creation_info = CreationInfo of Section 5.5.
5.2 StationMagnitude
mag = value of station magnitude, rounded to 2 decimal places
origin_id = id of origin used
mag_errors = None (until and unless we develop an appropriate estimator)
station_magnitude_type = “MN”
amplitude_id = id of amplitude used
method_id = None
waveform_id = None
creation_info = CreationInfo of Section 5.5
```

### 5.3 Magnitude

```
mag = value of network magnitude, rounded to 2 decimal places
mag_errors.uncertainty = standard deviation of station magnitudes, to 2 decimal places
magnitude_type = “MN”
origin_id = id of origin used
method_id = resource identifiers for methods mean, trimmed mean (%) or median
station_count = number of station magnitude contributions with non-zero weight
azimuthal_gap = largest difference between azimuths of station magnitude contributions with non-zero weight, to one decimal place
evaluation_mode = “manual” if any station magnitudes were manually omitted else “automatic”
evaluation_status = “preliminary” (may be edited by user later)
creation_info = CreationInfo of Section 5.5
5.4 StationMagnitudeContributions
Only station magnitudes which have passed quality control checks and which have not be explicitly omitted by the user should be included in the list of station magnitude contributions attached to a given network magnitude. Note that a weight of zero is only used to identify outliers that have been identified via trimmed-mean aggregation.
station_magnitude_id = id of associated station magnitude
residual = difference between this station magnitude and the network magnitude to 2 decimal places
weight = either 1 or 0
```

### 5.5 CreationInfo

```
agency_id = configurable agency name, e.g. “CHIS”
author = username if evaluation_mode = “manual” else process name
creation_time = now()
version = plugin name and version number of plugin
```

### 6. Regression Testing

Regression testing shall be integral to any new magnitude-calculation software
module.

A set of inputs and expected outputs shall be supplied which exercises as much
of the required functionality as possible. The test vector will consist of:

* A miniSEED file of waveforms for one or more events.
* A StationXML file of relevant station metadata.
* A QuakeML file containing descriptions of the events.
  * each event includes a preferred origin with a list of arrivals, lists of picks,
    arrivals, amplitudes, station magnitudes and network magnitudes
  * station magnitudes and network magnitudes must include some of the type being
    tested (“MN”)
  * network magnitudes must include a list of station magnitude contributions
    (the station magnitudes actually used) and a “method_id” (e.g. mean, trimmed
    mean or median)
  * picks will be available at some stations but not all
  * pick timing uncertainty shall be specified at some stations but not all
  * at some stations station magnitudes should be rejected for the following reasons
    * period too high
    * period too low
    * SNR too low
  * the origin shall specify a velocity model
* A travel-time table for the velocity model

The test will be considered "passed" if for each event the amplitudes, station
magnitudes and network magnitudes given in the QuakeML file are reproduced
exactly (to the specified precision).

### 7. References

* Bent, A., & Greene, H. (2014). Toward an Improved Understanding of the MN–Mw Time Dependence in Eastern Canada. Bulletin of the Seismological Society of America, 104(4), 2125-2132.
* Nanometrics, Inc. (1997, June). DAN User's Guide. Kanata, Ontario, Canada: Nanometrics, Inc.
* Nuttli, O. W. (1973). Seismic wave attenuation and magnitude relations for eastern North America. Journal of Geophysical Research, 78(5), 876-885.
* Schorlemmer, D., Euchner, F., Kästli, P., & Saul, J. (2011). QuakeML: status of the XML-based seismological data exchange format. Annals of Geophysics, 54(1), 59-65.
* Wetmiller, R. J., & Drysdale, J. A. (1982). Local Magnitudes of Eastern Canadian Earthquakes by an Extended Mb(Lg) Scale (abstract only). Earthquake Notes, 53(3), 40.


### Appendix A: Peak detection algorithm

Below is a Perl implementation of the peak detection algorithm described in Section 3.2.
This code was developed to work with BRTT Antelope software for seismic data analysis.

```perl
# $data is the windowed vertical component data.
# $amp is the return half maximum peak-to-peak amplitude
# $per is the return period of the amplitude measurement
# $tmax returned time of the measurement

	my $ipeak_save = -1;  # If > 0 indicates that at least 2 peaks or troughs
			  # have already been found.
			  # Stores position of 1st of pair of peak/trough
			  # for largest amp found so far.
	if ($npts > 3) {
		# $vel indicates up or down direction of signal
		# as part of search for peaks and troughs.
		my $ipeak = -1;  # will be > 0 and indicate position of peak or trough.
		# initialize direction in most cases. (a nonzero $vel is wanted.)
		my $vel = $data[2]-$data[1];
		for ($isamp = 2; $isamp < $npts-1; $isamp++) {
			my $vel2 = $data[$isamp+1]-$data[$isamp];
			if ( $vel2*$vel < 0.0 ) {
				# have found a peak or trough at $isamp.
				if($ipeak >= 0) {
					# have found consecutive peak and trough.
					my $amp_temp = 0.5 * abs ($data[$isamp] - $data[$ipeak]) ;
					if ($ipeak_save < 0 || $amp_temp > $amp ) {
						# Save this as the largest so far.
						$amp = $amp_temp;
						$per = 2.0 * ($isamp-$ipeak) * $dt ;
						$ipeak_save = $ipeak;
					}
				}
				# store location of current peak
				$ipeak = $isamp;
		                $vel = $vel2;
			}
			else {
				# re-initialize direction in case where first few samples equal.
				# This will only happen before first peak is found.
				if ($vel == 0)
				{
					$vel = $data[$isamp+1]-$data[$isamp];
				}
			}
		}
		$tmax = $t0 + $ipeak_save * $dt ;  # not really time of maximum
	}
```
