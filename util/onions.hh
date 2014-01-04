#pragma once

#include <string>
#include <map>
#include <list>
#include <iostream>
#include <vector>


typedef enum onion {
    oDET,
    oOPE,
    oAGG,
    oSWP,
    oPLAIN,
    oBESTEFFORT,
    oINVALID,
} onion;

//Sec levels ordered such that
// if a is less secure than b.
// a appears before b
// (note, this is not "iff")

enum class SECLEVEL {
    INVALID,
    PLAINVAL,
    OPE,
    DETJOIN,
    DET,
    SEARCH,
    HOM,
    RND,
};

//Onion layouts - initial structure of onions
typedef std::map<onion, std::vector<SECLEVEL> > onionlayout;

static onionlayout PLAIN_ONION_LAYOUT = {
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};

static onionlayout NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

static onionlayout BEST_EFFORT_NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})},
    // Requires SECLEVEL::DET, otherwise you will have to implement
    // encoding for negative numbers in SECLEVEL::RND.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};

static onionlayout STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                  // SECLEVEL::RND})}
};

static onionlayout BEST_EFFORT_STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
    //                              SECLEVEL::RND})},
    // HACK: RND_str expects the data to be a multiple of 16, so we use
    // DET (it supports decryption UDF) to handle the padding for us.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};

typedef std::map<onion, SECLEVEL>  OnionLevelMap;

enum class SECURITY_RATING {PLAIN, BEST_EFFORT, SENSITIVE};
