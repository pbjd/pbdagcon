
// Copyright (c) 2011-2015, Pacific Biosciences of California, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted (subject to the limitations in the
// disclaimer below) provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// * Neither the name of Pacific Biosciences nor the names of its
// contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY PACIFIC
// BIOSCIENCES AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.


#ifndef __GCON_DAZ_ALN_PROVIDER__
#define __GCON_DAZ_ALN_PROVIDER__

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <atomic>
#include "ProgramOpts.hpp"
#include "Alignment.hpp"
#include "AlnProvider.hpp"
#include "DazzlerAdapter.hpp"

// Represents one record from the LAS file, essentially a thin container for
// a dazzler overlap so we can manage things on the stack.
struct Record {
    Overlap ovl;
    std::vector<uint16> trace;

    Record() {}
    ~Record() {}

    Record(Record &&o) noexcept : 
        ovl(std::move(o.ovl)), 
        trace(std::move(o.trace)) {
        o.ovl.path.trace = NULL;
    }

    Record& operator=(Record &&o) noexcept {
        ovl = std::move(o.ovl);
        trace = std::move(o.trace);
        o.ovl.path.trace = NULL;
        return *this;
    }

    Record(const Record& o) : ovl(o.ovl), trace(o.trace) {
        ovl.path.trace = (void *) &trace.front();
    }

    Record& operator=(const Record& o) {
        ovl = o.ovl;
        trace = o.trace;
        ovl.path.trace = (void *) &trace.front();
        return *this;
    }

};

// Holds information for all the a,b overlaps in a particular direction,
// either forward or reverse. Overlaps for a particular a,b,strand 
// combination may come in as multiple overlaps.  This class allows us to
// handle them as a unit.
class TargetHit {
public:
    TargetHit();
    TargetHit(Record& rec);

    // Returns true if given overlap belongs to this hit, otherwise false
    bool belongs(Overlap& ovl);

    // Adds the next overlap to this hit set (a,b,strand combination)
    void add(Record& rec);

    void computeOvlScore(bool proper=false);

    int abeg();
    int aend();

    int alen;
    int blen;

    // Tracks the overlap score, (pct id) x (a-read aln len)
    float ovlScore;

    // Tracks the coverage score sum(1/depth for aln len)
    float covScore;

    // information promoted from the overlap struct
    int aread;
    int bread;
    uint32 flags;

    // container holding records related to this hit
    std::vector<Record> records;
};

std::ostream& operator<<(std::ostream& ostrm, TargetHit& hit);

// Re-usable class container for a target and its overlaps
class Target {
public:
    Target();
    Target(HITS_DB& db, int tspace, int small);
    ~Target();

    // Initializes this target based on the given record, possibly scoring as
    // a 'proper' overlap (more stringent).
    void firstRecord(Record& rec, bool proper=false);

    // Adds the next overlap record to this target, possibly scoring as 
    // a 'proper' overlap (more stringent).
    void addRecord(Record& rec, bool proper=false);

    // Sorts overlaps based on a two-phase scoring system
    void sortHits(bool sortCov);

    void getAlignments(std::vector<dagcon::Alignment> &alns, unsigned int max, bool sortCov);

    // ID of the target
    int id;

    // Length of the target
    int length; 

    std::vector<TargetHit> hits;

private:
    HITS_DB db_;
    char* abuffer_, *bbuffer_;
    Work_Data* work_;
    int tspace_, small_;
    std::vector<unsigned int> coverage_;
    bool needsFree_; // track if we need to free memory on destruct
};

///
/// Provides sets of alignments for a given target sequence from a daligner
/// output file.  
///
class DazAlnProvider : public AlnProvider {
public:
    /// Constructs a new alignment provider.  Checks the format of the file and
    /// throws an exception if it's malformed.
    /// \param popts options passed to the program on the command line
    DazAlnProvider(const ProgramOpts& popts);

    /// Cleans up some stuff.
    ~DazAlnProvider();

    /// Gets the set of alignments for the next target and puts them into the
    /// given vector.  Note this function will clear the contents of the vector
    /// prior to adding the next set of alignments.
    /// In dazzler parlance, this will correct the A reads as targets.
    /// \param dest reference to a vector to hold the alignments.
    /// \return True if there are more targets, otherwise false.
    bool nextTarget(std::vector<dagcon::Alignment>& dest);

    bool nextTarget(std::string& targSeq, std::vector<dagcon::Alignment>& dest);

private:
    // maintains the previous record
    Record prevRec_;
    Target* trg_;
    const ProgramOpts popts_;

    // Dazzler-related data 
    HITS_DB db_;
    int64 novl_, covl_;
    int tbytes_;
    FILE* input_;
    char* targSeqBuf_;

    /// Obtain data from the las file for the next overlap record
    void nextRecord(Record& rec);

};


/// Compares the hits based on (percent id) x (query alignment length)
bool cmpHitOvlScore(const TargetHit& l, const TargetHit& r); 

/// Compares based on coverage score
bool cmpHitCovScore(const TargetHit& l, const TargetHit& r); 

float invertedSum(float x, unsigned int y);

/// Convert dazzler alignment into a dagcon alignment. Eventually, we
/// should update the alignment graph to process the dazzler alignment
/// directly, but this will be useful for debugging purposes.
void decodeAlignment(Alignment* src, dagcon::Alignment& dest);

#endif //__GCON_DAZ_ALN_PROVIDER__
