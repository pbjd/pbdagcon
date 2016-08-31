#!/usr/bin/env python
"""Configure the build.

- Fetch boost/gtest.
- Create defines.mk
"""
import argparse
import commands
import contextlib
import os
import sys

ROOT = os.path.abspath(os.path.dirname(__file__))

def log(msg):
    sys.stderr.write(msg)
    sys.stderr.write('\n')

def shell(cmd):
    log(cmd)
    status, output = commands.getstatusoutput(cmd)
    if status:
        raise Exception('%d <-| %r' %(status, cmd))
    return output

def system(cmd):
    log(cmd)
    status = os.system(cmd)
    if status:
        raise Exception('%d <- %r' %(status, cmd))
    return

def mkdirs(path):
    if not os.path.isdir(path):
        os.makedirs(path)

@contextlib.contextmanager
def cd(nwd):
    cwd = os.getcwd()
    log('cd %r -> %r' %(cwd, nwd))
    os.chdir(nwd)
    yield
    os.chdir(cwd)
    log('cd %r <- %r' %(cwd, nwd))

def fetch_gtest(build_dir):
    gtest_version = 'release-1.7.0'
    gtest_uri = 'https://github.com/google/googletest.git'
    gdir = os.path.join(build_dir, 'test', 'cpp', gtest_version)
    if not os.path.isdir(gdir):
        get_gtest_cmd = 'git clone --branch %s --depth 1 %s %s' %(gtest_version, gtest_uri, gdir)
        system(get_gtest_cmd)
    assert os.path.isdir(gdir)
    return gdir

def fetch_boost_headers(build_dir):
    """Fetch into {build_dir}/src/cpp/third-party/
    Return actual directory path, relative to subdirs.
    """
    uri = 'https://www.dropbox.com/s/g22iayi83p5gbbq/boost_1_58_0-headersonly.tbz2?dl=0'
    hdir = os.path.join(build_dir, 'src', 'cpp', 'third-party', 'boost_1_58_0-headersonly')
    if not os.path.isdir(hdir):
        mkdirs(os.path.dirname(hdir))
        #get_boost_cmd = 'curl -L %s | tar xjf -C src/cpp/third-party -' %uri
        tbz = os.path.join(build_dir, 'src', 'cpp', 'third-party', 'boost_1_58_0-headersonly.tbz2')
        if not os.path.isfile(tbz):
            get_boost_cmd = 'curl -L %s --output %s' %(uri, tbz)
            system(get_boost_cmd)
        install_boost_cmd = 'tar vxjf %s -C %s/src/cpp/third-party | head' %(tbz, build_dir)
        system(install_boost_cmd)
    assert os.path.isdir(hdir)
    return hdir

def update_content(fn, content):
    current_content = open(fn).read() if os.path.exists(fn) else None
    if content != current_content:
        log('writing to %r' %fn)
        log('"""\n' + content + '"""')
        open(fn, 'w').write(content)

def compose_defines_with_hdf_headers(HDF_HEADERS):
    thisdir = os.path.dirname(os.path.abspath(__file__))
    return """
HDF_HEADERS:=%(HDF_HEADERS)s
#HDF5_INCLUDE?=${HDF_HEADERS}/src
CPPFLAGS+=-I${HDF_HEADERS}/src -I${HDF_HEADERS}/c++/src
CPPFLAGS+=-I../pbdata -I../hdf -I../alignment
LIBPBDATA_LIB     ?=../pbdata/libpbdata.so
LIBPBIHDF_LIB     ?=../pbdata/libpbihdf.so
LIBBLASR_LIB      ?=../pbdata/libblasr.so
"""%(dict(thisdir=thisdir, HDF_HEADERS=HDF_HEADERS))

def compose_defines():
    """
    Note that our local 'hdf' subdir will not even build
    in this case.
    """
    thisdir = os.path.dirname(os.path.abspath(__file__))
    return """
LIBPBDATA_INCLUDE ?=../pbdata
LIBPBIHDF_INCLUDE ?=../hdf
LIBBLASR_INCLUDE  ?=../alignment
LIBPBDATA_LIB     ?=%(thisdir)s/pbdata/libpbdata.so
LIBPBIHDF_LIB     ?=%(thisdir)s/pbdata/libpbihdf.so
LIBBLASR_LIB      ?=%(thisdir)s/pbdata/libblasr.so
nohdf             ?=1
"""%(dict(thisdir=thisdir))

def ifenvf(env, key, func):
    if key in env:
        return env[key]
    else:
        return func()
def setifenvf(envout, envin, key, func):
    envout[key] = ifenvf(envin, key, func)
def setifenv(envout, envin, key, val):
    envout[key] = envin.get(key, val)
def setenv(envout, key, val):
    envout[key] = val
def update_env_if(envout, envin, keys):
    for key in keys:
        if key in envin:
            envout[key] = envin[key]
def compose_defs_env(env):
    # We disallow env overrides for anything with a default from GNU make.
    nons = ['CXX', 'CC', 'AR'] # 'SHELL'?
    ovr    = ['%-20s ?= %s' %(k, v) for k,v in sorted(env.items()) if k not in nons]
    nonovr = ['%-20s := %s' %(k, v) for k,v in sorted(env.items()) if k in nons]
    return '\n'.join(ovr + nonovr + [''])
def compose_defines_pacbio(envin):
    """
    This is used by mobs via buildcntl.sh.
    """
    env = dict()
    #setifenv(env, envin, 'LIBPBDATA_INCLUDE', '../pbdata')
    #setifenv(env, envin, 'LIBPBIHDF_INCLUDE', '../hdf')
    #setifenv(env, envin, 'LIBBLASR_INCLUDE', '../alignment')
    #setifenv(env, envin, 'LIBPBDATA_LIB', '../pbdata/libpbdata.so')
    #setifenv(env, envin, 'LIBPBIHDF_LIB', '../hdf/libpbihdf.so')
    #setifenv(env, envin, 'LIBBLASR_LIB', '../alignment/libblasr.so')
    #setifenv(env, envin, 'nohdf', '1')
    possibs = set([
            'CC', 'CXX', 'AR',
            'GTEST_INCLUDE', 'GTEST_SRC',
            'LIBBLASR_INCLUDE', 'LIBBLASR_LIB', 'LIBBLASR_LIBFLAGS',
            'LIBPBDATA_INCLUDE', 'LIBPBDATA_LIB', 'LIBPBDATA_LIBFLAGS',
            'LIBPBIHDF_INCLUDE', 'LIBPBIHDF_LIB', 'LIBPBIHDF_LIBFLAGS',
            'HDF5_INCLUDE', 'HDF5_LIB', 'HDF5_LIBFLAGS',
            'PBBAM_INCLUDE', 'PBBAM_LIB', 'PBBAM_LIBFLAGS',
            'HTSLIB_INCLUDE', 'HTSLIB_LIB', 'HTSLIB_LIBFLAGS',
            'BOOST_INCLUDE','PTHREAD_LIBFLAGS',
            'ZLIB_LIB', 'ZLIB_LIBFLAGS',
            'GCC_LIB',
            'DAZZ_DB_SRC', 'DAZZ_DB_INCLUDE',
            'DALIGNER_SRC', 'DALIGNER_INCLUDE',
    ])
    update_env_if(env, envin, possibs)
    return compose_defs_env(env)

def configure_pacbio(envin, shared, build_dir):
    content1 = compose_defines_pacbio(envin)
    if shared:
        content1 += 'LDLIBS+=-lrt\n'
    update_content(os.path.join(build_dir, 'defines.mk'), content1)

def get_make_style_env(envin, args):
    envout = dict()
    for arg in args:
        if '=' in arg:
            k, v = arg.split('=')
            envout[k] = v
    envout.update(envin)
    return envout

class OsType:
    Unknown, Linux, Darwin = range(3)

def getOsType():
    uname = shell('uname -s')
    log('uname=%r' %uname)
    if 'Darwin' in uname:
        return OsType.Darwin
    elif 'Linux' in uname:
        return OsType.Linux
    else:
        return OsType.Unknown

def update_env_for_linux(env):
    env['SET_LIB_NAME'] = '-soname'
    env['SH_LIB_EXT'] = '.so'
    env['EXTRA_LDFLAGS'] = '-Wl,--no-as-needed'
def update_env_for_darwin(env):
    env['SET_LIB_NAME'] = '-install_name'
    env['SH_LIB_EXT'] = '.dylib'
    env['EXTRA_LDFLAGS'] = '-flat_namespace'
    # -flat_namespace makes BSD ld act like Linux ld, finding
    # shared libs recursively.
def update_env_for_unknown(env):
    env['SET_LIB_NAME'] = '-soname'
    env['SH_LIB_EXT'] = '.so'
update_env_for_os = {
    OsType.Linux: update_env_for_linux,
    OsType.Darwin: update_env_for_darwin,
    OsType.Unknown: update_env_for_unknown,
}


def parse_args(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--boost-headers', action='store_true',
            help='Download Boost headers.')
    parser.add_argument('--gtest', action='store_true',
            help='Download google-test.')
    parser.add_argument('--no-pbbam', action='store_true',
            help='Avoid compiling anything which would need pbbam.')
    parser.add_argument('--submodules', action='store_true',
            help='Set variables to use our git-submodules, which must be pulled and built first. (Implies --no-pbbam.)')
    parser.add_argument('--shared', action='store_true',
            help='Build for dynamic linking.')
    parser.add_argument('--mode', default='opt',
            help='debug, opt, profile [default=%(default)s] CURRENTLY IGNORED')
    parser.add_argument('--build-dir',
            help='Can be different from source directory, but only when *not* also building submodule.')
    parser.add_argument('makevars', nargs='*',
            help='Variables in the style of make: FOO=val1 BAR=val2 etc.')
    return parser.parse_args(args)

def set_defs_defaults(env, nopbbam):
    defaults = {
        'LIBPBDATA_LIBFLAGS': '-lpbdata',
        'LIBBLASR_LIBFLAGS':  '-lblasr',
        'SHELL': 'bash -xe',
        'DAZZ_DB_SRC': os.path.join(ROOT, '..', 'DAZZ_DB'),
        'DALIGNER_SRC': os.path.join(ROOT, '..', 'DALIGNER'),
        'DAZZ_DB_INCLUDE': '${DAZZ_DB_SRC}',
        'DALIGNER_INCLUDE': '${DALIGNER_SRC}',
        'PTHREAD_LIBFLAGS': '-lpthread',
    }
    pbbam_defaults = {
        'LIBPBIHDF_LIBFLAGS': '-lpbihdf',
        'PBBAM_LIBFLAGS': '-lpbbam',
        'HTSLIB_LIBFLAGS': '-lhts',
        'HDF5_LIBFLAGS': '-lhdf5_cpp -lhdf5',
        'ZLIB_LIBFLAGS': '-lz',
        'PTHREAD_LIBFLAGS': '-lpthread',
        'DL_LIBFLAGS': '-ldl', # neeeded by HDF5 always
    }
    if not nopbbam:
        defaults.update(pbbam_defaults)
    for k in defaults:
        if k not in env:
            env[k] = defaults[k]

def set_defs_submodule_defaults(env, nopbbam):
    libcpp = os.path.join(ROOT, 'blasr_libcpp')
    daligner = os.path.join(ROOT, 'DALIGNER')
    dazz_db = os.path.join(ROOT, 'DAZZ_DB')
    defaults = {
        'LIBPBDATA_INCLUDE': os.path.join(libcpp, 'pbdata'),
        'LIBBLASR_INCLUDE':  os.path.join(libcpp, 'alignment'),
        'LIBPBIHDF_INCLUDE': '' if nopbbam else os.path.join(libcpp, 'hdf'),
        'LIBPBDATA_LIB': os.path.join(libcpp, 'pbdata'),
        'LIBBLASR_LIB':  os.path.join(libcpp, 'alignment'),
        'LIBPBIHDF_LIB': '' if nopbbam else os.path.join(libcpp, 'hdf'),
        'DALIGNER_SRC': daligner,
        'DAZZ_DB_SRC': dazz_db,
    }
    for k in defaults:
        if k not in env:
            env[k] = defaults[k]

def write_makefile(build_dir_root, src_dir_root, makefilename, relpath):
    src_dir = os.path.join(src_dir_root, relpath)
    build_dir = os.path.join(build_dir_root, relpath)
    content = """\
vpath %%.cpp %(src_dir)s
vpath %%.c %(src_dir)s
include %(src_dir)s/%(makefilename)s
""" %dict(makefilename=makefilename, src_dir=src_dir)
    mkdirs(build_dir)
    fn = os.path.join(build_dir, makefilename)
    update_content(fn, content)

def write_makefiles(build_dir):
    write_makefile(build_dir, ROOT, 'makefile', '.')
    write_makefile(build_dir, ROOT, 'makefile', 'src/cpp')
    write_makefile(build_dir, ROOT, 'makefile', 'test/cpp')

def main(prog, *args):
    """We are still deciding what env-vars to use, if any.
    """
    conf = parse_args(args)
    envin = get_make_style_env(os.environ, conf.makevars)
    ost = getOsType()
    update_env_for_os[ost](envin)
    if conf.build_dir is not None:
        write_makefiles(conf.build_dir)
    else:
        conf.build_dir = '.'
    conf.build_dir = os.path.abspath(conf.build_dir)
    if conf.boost_headers:
        envin['BOOST_INCLUDE'] = fetch_boost_headers(conf.build_dir)
    if conf.gtest:
        gtest_dir = fetch_gtest(conf.build_dir)
        envin['GTEST_INCLUDE'] = os.path.join(gtest_dir, 'include')
        envin['GTEST_SRC'] = os.path.join(gtest_dir, 'src')
    if conf.submodules:
        set_defs_submodule_defaults(envin, conf.no_pbbam)
        conf.no_pbbam = True
    set_defs_defaults(envin, conf.no_pbbam)
    configure_pacbio(envin, conf.shared, conf.build_dir)


if __name__=="__main__":
    main(*sys.argv)
