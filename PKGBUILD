# Maintainer: Hal Clark <gmail.com[at]hdeanclark>
pkgname=dicomautomaton
pkgver=20190713_202000
pkgver() {
  date +%Y%m%d_%H%M%S
}
pkgrel=1

pkgdesc="Various tools for medical physics applications."
url="http://www.halclark.ca"
arch=('x86_64' 'i686' 'armv7h')
license=('unknown')
depends=(
   'gcc-libs'
   'ttf-computer-modern-fonts'
   'gnu-free-fonts'
   'zenity'
   'sfml'
   'jansson'
   'libpqxx'
   'postgresql'
   'nlopt' 
   'gsl'
   'boost-libs'
   'zlib'
   'cgal>=4.8'
   'wt'
   'explicator'
   'ygor'
)
optdepends=(
   'zenity'
   'dialog'
   'gnuplot'
   'patchelf'
   'bash-completion'
)
makedepends=(
   'cmake'
   'asio'
   'ygorclustering'
)
# conflicts=()
# replaces=()
# backup=()
# install='foo.install'
# source=("http://www.server.tld/${pkgname}-${pkgver}.tar.gz"
#         "foo.desktop")
# md5sums=(''
#          '')

#options=(!strip staticlibs)
options=(strip staticlibs)

build() {
  # ---------------- Configure -------------------
  # Default build with default compiler flags.
  cmake \
    -DMEMORY_CONSTRAINED_BUILD=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    ../

  ## Debug build with default compiler flags.
  #cmake \
  #  -DMEMORY_CONSTRAINED_BUILD=OFF \
  #  -DCMAKE_INSTALL_PREFIX=/usr \
  #  -DCMAKE_BUILD_TYPE=Debug \
  #  ../

  ## Custom build which can make use of custom compiler flags.
  ## (Note that the CMake build type must NOT be specified, or these flags will be overridden.)
  #cmake \
  #  -DMEMORY_CONSTRAINED_BUILD=OFF \
  #  -DCMAKE_INSTALL_PREFIX=/usr \
  #  -DCMAKE_CXX_FLAGS='-O2' \
  #  ../

  # ------------------ Build ---------------------
  # Scale compilation, but limit to 8 concurrent jobs to temper memory usage.
  JOBS=$(nproc)
  JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))
  make -j "$JOBS" VERBOSE=1

  ## Build with a single job to keep memory usage low.
  #make VERBOSE=1

  ## Debug compiler flags without actually compiling ("dry-run").
  #make -n

}

package() {
  make DESTDIR="${pkgdir}" install
}

# vim:set ts=2 sw=2 et:
