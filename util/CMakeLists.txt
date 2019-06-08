cmake_minimum_required(VERSION 3.14)
project(TShell C)

aux_source_directory(. DIR_UTIL)

add_library(util ${DIR_UTIL})