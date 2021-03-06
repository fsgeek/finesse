
testiowrap_sources = [
    'log_normal.c',
    'testiowrapper.c'
]

m_dep = cc.find_library('m', required : false)


executable('testiowrap',
            [testiowrap_sources],
            dependencies : [deps, munit, m_dep],
            include_directories : [include_dirs],
            link_with : [libiowrapper])
