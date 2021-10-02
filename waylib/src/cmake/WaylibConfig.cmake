foreach(module ${Waylib_FIND_COMPONENTS})
    find_package(Waylib${module})
endforeach()
