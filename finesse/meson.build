protoc = find_program('protoc', required : false)
dep = dependency('protobuf', required : false)

if not protoc.found() or not dep.found()
  error('MESON_SKIP_TEST: protoc tool and/or protobuf pkg-config dependency not found')
endif


# pkg-config --cflags 'libprotobuf-c >= 1.0.0'
# pkg-config --libs 'libprotobuf-c >= 1.0.0'
# soup = dependency('libsoup-2.4', version : '>= 2.24')

protoc_c_cfg = run_command('pkg-config', '--cflags', 'libprotobuf-c >= 1.0.0')
protoc_c_cflags = protoc_c_cfg.stdout().strip()
protoc_c_lib = dependency('libprotobuf-c', version: '>= 1.0.0')


gen = generator(protoc, \
 output    : ['@BASENAME@.pb-c.c', '@BASENAME@.pb-c.h'],
 arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--c_out=@BUILD_DIR@', '@INPUT@'])

generated = gen.process('finesse.proto')

libfinesse_sources = ['dirmap.c',
                      'fdmgr.c',
                      'finesse.c',
                      'finesse-fuse.c',
                      'finesse-lookup.c',
                      # 'finesse-search.c',
                      'init.c',
                      'mqcomm.c',
                      'name_map.c',
                      'openclose.c',
                      'pathsearch.c', 
                      'test.c',
                      generated
                      ]

# source - https://stackoverflow.com/questions/13939904/how-to-use-protocol-buffers-with-autoconf-automake

#%.pb-c.c %.pb-c.h: %.proto
#        $(PROTOC) --proto_path=$(srcdir) --c_out=$(builddir) $^

#%.pb.cc %.pb.h: %.proto
#        $(PROTOC) --proto_path=$(srcdir) --cpp_out=$(builddir) $^

deps = [ thread_dep ]
libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libfuse = cc.find_library('libfuse')

libfinesse = library('finesse', libfinesse_sources, 
                version: meson.project_version(),
                soversion: '3', 
                include_directories: include_dirs,
                dependencies: [deps, librt, libuuid, protoc_c_lib, libfuse], 
                install: true,
                c_args: [ '-DFUSE_USE_VERSION=32', protoc_c_cflags])

