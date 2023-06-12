#!/usr/bin/env python3

from setuptools import setup

setup(
    name="tiledwebmaps",
    version="0.1.0",
    description="A small library for retrieving map images from a tile provider with arbitrary resolution, location and bearing.",
    author="Florian Fervers",
    author_email="florian.fervers@gmail.com",
    packages=["tiledwebmaps"],
    package_data={"tiledwebmaps": ["*.so"]},
    license="MIT",
    install_requires=[
        "numpy",
        "cosy1",
        "pyyaml",
        "requests",
    ],
    url="https://github.com/fferflo/tiledwebmaps",
)
