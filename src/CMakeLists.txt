### DO NOT DELETE THIS COMMENT: INSERT_ARCHETYPES_HERE ###
USE_CYCLUS("flexicamore" "enrichment")
USE_CYCLUS("flexicamore" "source")
USE_CYCLUS("flexicamore" "flexible_input")

INSTALL_CYCLUS_MODULE("flexicamore" "")

# install header files
FILE(GLOB h_files "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
