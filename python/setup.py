import setuptools

package_name = "blinkytools"

# import description from readme file
with open("README.md", "r") as fh:
    long_description = fh.read()

# import version from package internal variable
with open(package_name + "/version.py") as f:
    exec(f.read())

setuptools.setup(
    name=package_name,
    version=__version__,
    author="Robin Scheibler",
    author_email="fakufaku@gmail.com",
    description="Software tools to work with Blinky sound-to-light conversion sensors.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/onolab-tmu/blinky",
    install_requires=["numpy", "matplotlib", "msgpack", "opencv-python", "pillow"],
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.6",
)
