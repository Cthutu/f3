//----------------------------------------------------------------------------------------------------------------------
// Miscellaneous utilities
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <string>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// String manipulation
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

class CmdLine;

func split(const std::string& text, std::string&& delim) -> std::vector<std::string>;
func join(const std::vector<std::string>& elems, std::string&& delim) -> std::string;
func join(const std::vector<std::filesystem::path>& elems, std::string&& delim) -> std::string;

func ltrim(std::string& s) -> void;
func rtrim(std::string& s) -> void;
func trim(std::string& s) -> void;

func extractSubStr(const std::string& str, char startDelim, char endDelim) -> std::string;

func validateFileName(const std::string& str) -> bool;
func ensurePath(const CmdLine& cmdLine, std::filesystem::path&& path) -> bool;

func generateGuid() -> std::string;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
