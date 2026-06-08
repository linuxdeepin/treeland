include(CMakeFindDependencyMacro)
foreach(module ${Waylib_FIND_COMPONENTS})
    find_dependency(Waylib${module})
endforeach()

