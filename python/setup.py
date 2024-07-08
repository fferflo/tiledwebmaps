#!/usr/bin/env python3

from setuptools import setup

setup(
    name="tiledwebmaps",
    version="0.1.3",
    description="A lightweight library for retrieving map images from a tile provider with custom resolution, location and bearing.",
    author="Florian Fervers",
    author_email="florian.fervers@gmail.com",
    packages=["tiledwebmaps"],
    package_data={"tiledwebmaps": [
        "*.so",
        "proj_data/*",
    ]},
    license="MIT",
    install_requires=[
        "numpy",
        "pyyaml",
    ],
    extras_require={
        "scripts": [
            "requests",
            "dbfread",
            "pandas",
            "openpyxl",
            "pyunpack",
            "imageio",
            "tqdm",
            "tinypl",
            "beautifulsoup4",
            "pylibjpeg-openjpeg",
        ],
    },
    url="https://github.com/fferflo/tiledwebmaps",
)
