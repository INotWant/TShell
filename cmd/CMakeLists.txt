cmake_minimum_required(VERSION 3.14)
project(TShell C)

aux_source_directory(. DIR_CMD)

add_library(cmd ${DIR_CMD})
