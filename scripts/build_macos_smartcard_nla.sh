#----------------------------------------------------------------------------------------------------------------------------

#!/bin/bash -eu

# install dependencies
brew install opensc pkcs11-helper krb5

# kerberos
export GSS_ROOT_FLAVOUR=MIT
# set pkgconfig path to brew openssl
export PKG_CONFIG_PATH=/usr/local/opt/openssl/lib/pkgconfig

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
    libs=( /usr/local/lib /usr/lib )
    export LDFLAGS=$(trim_spaces "${LDFLAGS:-} $(printf -- '-L%s '  "${libs[@]}")")
    export LD_LIBRARY_PATH=$(trim_colons      $(printf -- '%s:'    "${libs[@]}"))
    export CFLAGS=$(trim_spaces "${LDFLAGS:-} -I/usr/local/opt/pkcs11-helper/include")

    # brew openssl and pkcs11-helper
    export PKG_CONFIG_PATH=/usr/local/opt/openssl/lib/pkgconfig:/usr/local/opt/pkcs11-helper/lib/pkgconfig

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
        -DWITH_PKCS11H=ON  \
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
	    . \
	    && make \
	    && make CTEST_OUTPUT_ON_FAILURE=1 test
}

function cmake_cleanup(){

  make clean
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
