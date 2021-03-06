SET(TTT_IASP91_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/iasp91.tbl
	${CMAKE_CURRENT_BINARY_DIR}/iasp91.hed
)

SET_SOURCE_FILES_PROPERTIES(${TTT_IASP91_GENERATED} PROPERTIES GENERATED 1)


SET(TTT_AK135_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/ak135.tbl
	${CMAKE_CURRENT_BINARY_DIR}/ak135.hed
)

SET_SOURCE_FILES_PROPERTIES(${TTT_AK135_GENERATED} PROPERTIES GENERATED 1)


SET(TTTDATA_DATA
	${TTT_IASP91_GENERATED}
	${TTT_AK135_GENERATED}
	iasp91.tvel
	ak135.tvel
)

ADD_CUSTOM_TARGET(TTT_iasp91 ALL DEPENDS tau_remodl tau_setbrn)
ADD_CUSTOM_TARGET(TTT_ak135 ALL DEPENDS tau_remodl tau_setbrn)

IF (WIN32)
	MESSAGE(STATUS "Calling remodl and setbrn from ${CMAKE_CURRENT_SOURCE_DIR}")
	ADD_CUSTOM_COMMAND(
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tau_remodl iasp91 ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS tau_remodl
		OUTPUTS remodl.tbl remodl.hed
		TARGET  TTT_iasp91
	)

	ADD_CUSTOM_COMMAND(
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tau_setbrn iasp91
		DEPENDS tau_setbrn remodl.tbl remodl.hed
		OUTPUTS iasp91.tbl iasp91.hed
		TARGET  TTT_iasp91
	)

	ADD_CUSTOM_COMMAND(
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tau_remodl ak135 ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS tau_remodl
		OUTPUTS remodl.tbl remodl.hed
		TARGET  TTT_ak135
	)

	ADD_CUSTOM_COMMAND(
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tau_setbrn ak135
		DEPENDS tau_setbrn remodl.tbl remodl.hed
		OUTPUTS ak135.tbl ak135.hed
		TARGET  TTT_ak135
	)
ELSE (WIN32)
	IF("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" STRLESS "2.6")
			SET(TAU_COMMAND_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/")
	ELSE("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" STRLESS "2.6")
			SET(TAU_COMMAND_PREFIX "")
	ENDIF("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" STRLESS "2.6")
	IF(NOT CMAKE_HOST_ARCHITECTURE)
		ADD_CUSTOM_COMMAND(
			COMMAND ${TAU_COMMAND_PREFIX}tau_remodl iasp91 ${CMAKE_CURRENT_SOURCE_DIR}
			DEPENDS tau_remodl
			OUTPUTS remodl.tbl remodl.hed
			TARGET  TTT_iasp91
		)

		ADD_CUSTOM_COMMAND(
			COMMAND ${TAU_COMMAND_PREFIX}tau_setbrn iasp91
			DEPENDS tau_setbrn remodl.tbl remodl.hed
			OUTPUTS iasp91.tbl iasp91.hed
			TARGET  TTT_iasp91
		)

		ADD_CUSTOM_COMMAND(
			COMMAND ${TAU_COMMAND_PREFIX}tau_remodl ak135 ${CMAKE_CURRENT_SOURCE_DIR}
			DEPENDS tau_remodl
			OUTPUTS remodl.tbl remodl.hed
			TARGET  TTT_ak135
		)

		ADD_CUSTOM_COMMAND(
			COMMAND ${TAU_COMMAND_PREFIX}tau_setbrn ak135
			DEPENDS tau_setbrn remodl.tbl remodl.hed
			OUTPUTS ak135.tbl ak135.hed
			TARGET  TTT_ak135
		)
	ENDIF(NOT CMAKE_HOST_ARCHITECTURE)
ENDIF (WIN32)

ADD_CUSTOM_TARGET(
	TTTData
	DEPENDS tau_remodl tau_setbrn
	ADD_DEPENDENCIES TTT_iasp91 TTT_ak135
)

SET(
	CLEAN_FILES
		${TTT_IASP91_GENERATED}
		${TTT_AK135_GENERATED}
		remodl.hed
		remodl.tbl
		remodl1.lis
		remodl2.lis
		setbrn1.lis
		setbrn2.lis
		setbrn3.lis
)

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CLEAN_FILES}")


IF(NOT CMAKE_HOST_ARCHITECTURE)
	SC_INSTALL_DATA(TTTDATA ttt)
ENDIF(NOT CMAKE_HOST_ARCHITECTURE)
