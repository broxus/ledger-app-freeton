# shellcheck disable=SC1091,SC2155

# SOURCE THIS FILE
# . prepare-devenv s|x

if [ $# -ne 1 ]; then
    echo "Possible options: s or x"
    exit 1
elif [[ $1 == "-h" ]]; then
    echo "Possible options: s or x"
    exit 1
elif [[ $1 != "s" ]] && [[ $1 != "x" ]]; then
    echo "Possible options: blue, s or x"
    exit 1
fi

if [[ $(dpkg-query -s python3.8-venv 2>&1) == *'is not installed'* ]]; then
    printf "\nPackage python3-venv is missing.\nOn Ubuntu distros, run:\n\sudo apt-get install python3.8-venv\n\n"
    exit 1
fi

if [[ $(cat /etc/udev/rules.d/20-hw1.rules) == *'ATTRS{idVendor}=="2c97", ATTRS{idProduct}=="0004"'* ]]; then
    printf "\nMissing udev rules. Please refer to https://support.ledger.com/hc/en-us/articles/115005165269-Fix-connection-issues\n\n"
    exit 1
fi


if [ ! -d dev-env ]; then
    mkdir dev-env
    mkdir dev-env/SDK
    mkdir dev-env/CC
    mkdir dev-env/CC/others
    mkdir dev-env/CC/nanox

    wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2 -O gcc-arm-none-eabi.tar.bz2
    tar xf gcc-arm-none-eabi.tar.bz2
    rm gcc-arm-none-eabi.tar.bz2
    cp -r gcc-arm-none-eabi* dev-env/CC/nanox/gcc-arm-none-eabi
    mv gcc-arm-none-eabi* dev-env/CC/others/gcc-arm-none-eabi

    wget http://releases.llvm.org/9.0.0/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz -O clang+llvm.tar.xz
    tar xf clang+llvm.tar.xz
    rm clang+llvm.tar.xz
    mv clang+llvm* dev-env/CC/others/clang-arm-fropi

    wget https://github.com/LedgerHQ/nanos-secure-sdk/archive/2.0.0-1.tar.gz -O nanos-secure-sdk.tar.gz
    tar xf nanos-secure-sdk.tar.gz
    rm nanos-secure-sdk.tar.gz
    mv nanos-secure-sdk* dev-env/SDK/nanos-secure-sdk

    wget https://github.com/LedgerHQ/nanox-secure-sdk/archive/1.2.4-5.1.tar.gz -O nanox-secure-sdk.tar.gz
    tar xf nanox-secure-sdk.tar.gz
    rm nanox-secure-sdk.tar.gz
    mv nanox-secure-sdk* dev-env/SDK/nanox-secure-sdk

    python3.8 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
fi

# Temp decision to avoid rebuild set of compilers
python3.8 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

#source venv/bin/activate

if [[ $1 == "s" ]]; then
    export BOLOS_SDK=$(pwd)/dev-env/SDK/nanos-secure-sdk
    export BOLOS_ENV=$(pwd)/dev-env/CC/others
elif [[ $1 == "x" ]]; then
    export BOLOS_SDK=$(pwd)/dev-env/SDK/nanox-secure-sdk
    export BOLOS_ENV=$(pwd)/dev-env/CC/nanox
fi
