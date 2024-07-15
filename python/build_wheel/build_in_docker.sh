#!/bin/bash
set -e -u -x

$BUILD_PYTHON_ROOT_PATH/bin/python -m pip install cython numpy

yum install -y openssl-devel libtiff-devel blas-devel lapack-devel

git clone https://github.com/opencv/opencv && cd opencv && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_opencv_python=OFF -DBUILD_opencv_dnn=OFF -DBUILD_opencv_video=OFF -DBUILD_opencv_highgui=OFF -DBUILD_opencv_ml=OFF -DBUILD_opencv_flann=OFF -DBUILD_opencv_video=OFF -DBUILD_opencv_videoio=OFF -DBUILD_opencv_features2d=OFF -DBUILD_opencv_gapi=OFF -DBUILD_opencv_photo=OFF -DCMAKE_CXX_STANDARD=14 -DWITH_CUDA=OFF -DCUDA_FAST_MATH=ON -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DBUILD_opencv_apps=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_PROTOBUF=OFF -DWITH_PROTOBUF=OFF -DWITH_VTK=OFF -DWITH_GTK=OFF -DBUILD_JAVA=OFF -DWITH_QUIRC=OFF -DWITH_ADE=OFF .. && make -j32 && make install -j32 && cd ../.. && rm -rf opencv
git clone https://github.com/curl/curl && cd curl && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none . && make -j32 && make install -j32 && cd .. && rm -rf curl
git clone https://github.com/JosephP91/curlcpp && cd curlcpp && cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCURLCPP_USE_PKGCONFIG=OFF . && make -j32 && make install -j32 && cd .. && rm -rf curlcpp
git clone https://github.com/pybind/pybind11 && cd pybind11 && cmake -DCMAKE_BUILD_TYPE=Release -DPYBIND11_TEST=OFF -DPYTHON_EXECUTABLE=/usr/local/bin/python3.$BUILD_MINOR -DPYBIND11_PYTHON_VERSION=3.$BUILD_MINOR . && make -j32 && make install -j32 && cd .. && rm -rf pybind11
git clone https://github.com/OSGeo/PROJ && cd PROJ && cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF . && make -j32 && make install -j32 && cd .. && rm -rf PROJ
git clone https://github.com/gulrak/filesystem && cd filesystem && cmake -DCMAKE_BUILD_TYPE=Release -DGHC_FILESYSTEM_BUILD_TESTING=OFF -DGHC_FILESYSTEM_BUILD_EXAMPLES=OFF -DGHC_FILESYSTEM_WITH_INSTALL=ON . && make -j32 && make install -j32 && cd .. && rm -rf filesystem
git clone https://github.com/xtensor-stack/xtl && cd xtl && cmake -DCMAKE_BUILD_TYPE=Release . && make -j32 && make install -j32 && cd .. && rm -rf xtl
git clone https://github.com/xtensor-stack/xtensor && cd xtensor && cmake -DCMAKE_BUILD_TYPE=Release . && make -j32 && make install -j32 && cd .. && rm -rf xtensor
git clone https://github.com/xtensor-stack/xtensor-io && cd xtensor-io && cmake -DCMAKE_BUILD_TYPE=Release . && make -j32 && make install -j32 && cd .. && rm -rf xtensor-io
git clone https://github.com/xtensor-stack/xtensor-python && cd xtensor-python && cmake -DCMAKE_BUILD_TYPE=Release -DPYTHON_EXECUTABLE=/usr/local/bin/python3.$BUILD_MINOR -DPYBIND11_PYTHON_VERSION=3.$BUILD_MINOR . && make -j32 && make install -j32 && cd .. && rm -rf xtensor-python
git clone https://github.com/xtensor-stack/xtensor-blas && cd xtensor-blas && cmake -DCMAKE_BUILD_TYPE=Release . && make -j32 && make install -j32 && cd .. && rm -rf xtensor-blas
git clone https://github.com/catchorg/Catch2 && cd Catch2 && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j32 && make install -j32 && cd ../.. && rm -rf Catch2
git clone https://github.com/fferflo/xtensor-interfaces && cd xtensor-interfaces && cmake -DCMAKE_BUILD_TYPE=Release . && make -j32 && make install -j32 && cd .. && rm -rf xtensor-interfaces


cp -r /tiledwebmaps /tiledwebmaps2

cd /tiledwebmaps2 && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DPython_ROOT_DIR=$BUILD_PYTHON_ROOT_PATH ..
make -j32
cd python

$BUILD_PYTHON_ROOT_PATH/bin/python setup.py bdist_wheel
rename py3- cp3$BUILD_MINOR- dist/*.whl
auditwheel repair dist/*.whl --plat $BUILD_PLATFORM
cp wheelhouse/* /io