#----------------------------------------------------------------------------------------------------------------------------

#!/bin/bash -eu

freerdp_deps=(

    build-essential git-core debhelper cdbs dpkg-dev
    autotools-dev cmake pkg-config xmlto libssl-dev docbook-xsl
    xsltproc libxkbfile-dev libx11-dev libwayland-dev libxrandr-dev
    libxi-dev libxrender-dev libxext-dev libxinerama-dev libxfixes-dev
    libxcursor-dev libxv-dev libxdamage-dev libxtst-dev libcups2-dev
    libpcsclite-dev libasound2-dev libpulse-dev libjpeg-dev
    libgsm1-dev libusb-1.0-0-dev libudev-dev libdbus-glib-1-dev
    uuid-dev libxml2-dev
    libavutil-dev libavcodec-dev libavresample-dev libcunit1-dev libdirectfb-dev xmlto
    doxygen libxtst-dev


    # pkcs#11 stuff:
    opencryptoki opensc
    libopencryptoki-dev libopencryptoki0
    gnupg-pkcs11-scd
    libpkcs11-helper1-dev libpkcs11-helper1
    libp11-dev libp11-kit-dev libp11-kit0 p11-kit p11-kit-modules
    pkcs11-dump pkcs11-data

    # kerberos
    krb5-config krb5-doc krb5-locales krb5-multidev
    libkrb5-3 libkrb5-dbg libkrb5-dev libkrb5support0

    krb5-admin-server krb5-auth-dialog krb5-gss-samples  krb5-kdc krb5-kdc-ldap
    krb5-otp krb5-pkinit krb5-strength krb5-sync-plugin krb5-sync-tools krb5-user

    # gssapi
    libgssapi-krb5-2
    libgss-dbg libgss-dev libgss3

    $(~/bin/distribution | ( read sys dist release ;
	                         if [ "$dist" = ubuntu ] ; then
		                         case "$release" in
		                         (18.04)
			                         packs=(
			                             opensc-pkcs11 gnupg-pkcs11-scd-proxy
			                             libengine-pkcs11-openssl1.1

			                             ykcs11 yubico-piv-tool

			                             libkdb5-9
			                             libkadm5clnt-mit11 libkadm5srv-mit11
			                             krb5-k5tls krb5-kpropd

			                             gss-ntlmssp gss-ntlmssp-dev

			                             libgstreamer-gl1.0-0 libgstreamer-plugins-base1.0-0 libgstreamer-plugins-base1.0-dev
			                             libgstreamer-plugins-good1.0-0 libgstreamer-plugins-good1.0-dev libgstreamer1.0-0
			                             libgstreamer1.0-0-dbg libgstreamer1.0-dev libgstreamermm-1.0-1 libgstreamermm-1.0-dev
			                             libgstreamermm-1.0-doc
			                         )
			                         ;;
		                         (*)
			                         packs=(
			                             libgstreamer1.0-dev libgstreamer0.10-dev
			                             libgstreamer-plugins-base1.0-dev libgstreamer-plugins-base0.10-dev
			                         )
			                         ;;
		                         esac
	                         else
		                         packs=(
		                             libgstreamer1.0-dev libgstreamer0.10-dev
		                             libgstreamer-plugins-base1.0-dev libgstreamer-plugins-base0.10-dev
		                         )
	                         fi
	                         echo ${packs[@]}
	  ) )
)


function install_deps(){
    echo "sudo apt-get install ${freerdp_deps[@]}"
}




sanitize_ld=(
        -fsanitize=leak
)

sanitize_cc=(
    -fsanitize=address
    -fsanitize=null
    -fsanitize=bounds
    -fsanitize=vla-bound
    -fsanitize=object-size

    -fsanitize=unreachable
    -fsanitize=return # C++ only

    # -fsanitize=shift
    # -fsanitize=shift-exponent
    # -fsanitize=shift-base
    # -fsanitize=integer-divide-by-zero
    # -fsanitize=signed-integer-overflow

    -fsanitize=float-divide-by-zero
    -fsanitize=float-cast-overflow
    -fsanitize=nonnull-attribute
    -fsanitize=returns-nonnull-attribute
    -fsanitize=bool
    -fsanitize=enum
    -fsanitize=vptr # C++ only


    -fsanitize-address-use-after-scope
    -fsanitize-undefined-trap-on-error
    -fstack-protector-all
    -fstack-check
)


function trim_spaces(){
    local string="$1"
    # ${a## } or ${a%% } doesn't work! (in bash 4.3).
    string="$(echo "$string" | sed -e 's/^ *//' -e 's/ *$//')"
    echo -n "${string}"
}

function trim_colons(){
    local string="$1"
    string="$(echo "$string" | sed -e 's/^:*//' -e 's/:*$//')"
    echo -n "${string}"
}

function with_gcc_8(){
    gcc_prefix="/usr/local/gcc"
    libs=(${gcc_prefix}/lib64 ${gcc_prefix}/lib /usr/local/lib64 /usr/local/lib /usr/lib /lib )
    export PATH=$(trim_colons "${gcc_prefix}/bin:${PATH}")
    export CFLAGS=$(trim_spaces "-I${gcc_prefix}/include ${CFLAGS:-}")
    export CXXFLAGS=$(trim_spaces "-I${gcc_prefix}/include ${CXXFLAGS:-}")
    export LDFLAGS=$(trim_spaces "${LDFLAGS:-} $(printf -- '-L%s '  "${libs[@]}")")
    export LD_LIBRARY_PATH=$(trim_colons      $(printf -- '%s:'    "${libs[@]}"))
    export PKG_CONFIG_PATH="${gcc_prefix}/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/opt/openssl/lib/pkgconfig"

    gcc -dumpversion
    case "$(gcc -dumpversion)" in
    [789]*)
        export CFLAGS=$(trim_spaces "${CFLAGS:-} ${sanitize_cc[*]} -g -g3 -ggdb -O0")
        export LDFLAGS=$(trim_spaces "${LDFLAGS:-} ${sanitize_ld[*]}")
        ;;
    *)
        export CFLAGS=$(trim_spaces "${CFLAGS:-} -g -g3 -ggdb -O0")
        export LDFLAGS=$(trim_spaces "${LDFLAGS:-}")
        ;;
    esac

    for var in PATH CFLAGS CXXFLAGS LDFLAGS LD_LIBRARY_PATH PKG_CONFIG_PATH ; do
        printf '%-24s = %s\n' "${var}" "${!var}"
    done
}

function compile_and_test(){
    cmake \
	    -DWITH_SSE2=ON    \
        -DWITH_GSSAPI=ON   \
        -DWITH_PKCS11H=ON   \
        -DWITH_KERBEROS=ON   \
        -DWITH_LIBSYSTEMD=OFF \
	    \
	    -DWITH_CUNIT=ON         \
	    -DBUILD_TESTING=ON       \
	    -DCMAKE_BUILD_TYPE=Debug  \
	    \
	    -DWITH_DEBUG_NTLM=ON \
	    -DWITH_DEBUG_NEGO=ON  \
	    -DWITH_DEBUG_NLA=ON    \
	    -DWITH_DEBUG_SCARD=ON   \
	    -DWITH_SMARTCARD_INSPECT=ON   \
	    . \
	    && make \
	    && make CTEST_OUTPUT_ON_FAILURE=1 test
}

function cmake_cleanup(){

  # clean up cached cmakefiles
  find . -name CTestTestfile.cmake | xargs rm
  find . -name cmake_install.cmake | xargs rm
  find . -name CMakeCache.txt | xargs rm
  find . -name CMakeFiles | xargs rm -r
  find . -name Makefile | xargs rm

}

function main(){
    cmake_cleanup
    with_gcc_8

    # install_deps
    compile_and_test

    printf "\nUse: \n%s\n\n" "export ASAN_OPTIONS=verbosity=1:debug=true:check_initialization_order=true:detect_stack_use_after_return=true:strict_string_checks=true"
    exit 0
}

main "$@"

#----------------------------------------------------------------------------------------------------------------------------
