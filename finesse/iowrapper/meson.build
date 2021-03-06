
iowrap_api_SOURCES = ['iowrapper.c']
iowrapper_library_SOURCES = ['shared.c']

libiowrap = static_library('io_wrap',
                            iowrap_api_SOURCES,
                            include_directories : include_dirs,
                            dependencies: deps,
                            install: true,
                            )


libiowrapper = library('io_wrapper',
                        iowrapper_library_SOURCES,
                        version: meson.project_version(),
                        soversion: '1',
                        include_directories : include_dirs,
                        dependencies: deps,
                        link_with: libiowrap,
                        install: true)


subdir('tests')
