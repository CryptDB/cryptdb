#pragma once

#include <memory>

#include <main/Connect.hh>

bool
loadStoredProcedures(const std::unique_ptr<Connect> &conn);

std::vector<std::string>
getStoredProcedures();

