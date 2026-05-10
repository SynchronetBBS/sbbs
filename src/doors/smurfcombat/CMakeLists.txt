cmake_minimum_required(VERSION 3.10)
project(smurfcombat C)

set(SOURCES
    mdel.c
    smurf.c
    smurfarn.c
    smurfblk.c
    smurfcxx.c
    smurfdxx.c
    smurfetc.c
    smurfgen.c
    smurfhst.c
    smurfini.c
    smurfins.c
    smurfmnt.c
    smurfmod.c
    smurfmss.c
    smurfnxx.c
    smurfreg.c
    smurfrnz.c
    smurfset.c
    smurfsrv.c
    smurfuar.c
    smurfwin.c
)

include_directories(
    ${CMAKE_SOURCE_DIR}/../../odoors
    ${CMAKE_SOURCE_DIR}/../../conio
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(smurf ${SOURCES})

target_link_libraries(smurf
    ODoors
)