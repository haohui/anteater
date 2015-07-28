# Find The executable of Yices SMT solver

IF (Yices_EXECUTABLE)
  SET(Yices_FOUND TRUE)
ENDIF (Yices_EXECUTABLE)

FIND_PROGRAM(Yices_EXECUTABLE NAMES yices DOC "Yices Executable")

IF (Yices_EXECUTABLE)
  SET (Yices_FOUND TRUE)
ELSE ()
  SET (Yices_FOUND FALSE)
ENDIF (Yices_EXECUTABLE)

IF (Yices_FOUND)
  IF (NOT Yices_FIND_QUIETLY)
    MESSAGE(STATUS "Found Yices: ${Yices_EXECUTABLE}")
  ENDIF (NOT Yices_FIND_QUIETLY)

ELSE ()
  IF (Yices_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find Yices!")
  ENDIF()
ENDIF()


