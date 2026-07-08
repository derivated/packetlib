from setuptools import setup, find_packages

setup(
    name="packetlib",
    version="0.1.0",
    packages=find_packages(),
    python_requires=">=3.7",
    author="Linus Reynolds",
    description="A Python library to evaluate and measure timestamps of HTTPS requests using extensible C++ backends.",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
    ],
)
