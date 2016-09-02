#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>
#include "DazAlnProvider.hpp"
#include "SimpleAligner.hpp"

#undef DEBUG

using namespace PacBio::DagCon; // for IOException

IOException::IOException(const char* desc) : desc_(desc) {}

static char ToU[8] = { 'A', 'C', 'G', 'T', '.', '[', ']', '-' };
static int BORDER = 10;

// Should write my own, but for reasons of expediency, this borrows heavily
// from LAshow.c
DazAlnProvider::DazAlnProvider(const ProgramOpts& popts) :
    popts_(popts),
    novl_(0),
    covl_(0) {

    // Initialize the sequence DB accessor

    // Open_DB takes a mutable char pointer, booo
    char* path = new char[popts_.seqFile.length()+1];
    std::strcpy(path,popts_.seqFile.c_str());
    path[popts_.seqFile.length()] = '\0';
    int status = Open_DB(path, &db_);
    if (status < 0)
        throw IOException("Failed to open DB");

    delete[] path;

    Trim_DB(&db_);

    // Initialize the alignment file reader
    input_ = fopen(popts_.alnFile.c_str(),"r");
    if (input_ == NULL) {
        std::string msg = "Open failed: " + popts_.alnFile;
        throw IOException(msg.c_str());
    }

    if (fread(&novl_,sizeof(int64),1,input_) != 1)
        throw IOException("Failed to read novl");

    int tspace;
    if (fread(&tspace,sizeof(int),1,input_) != 1)
        throw IOException("Failed to read tspace");

    int small;
    if (tspace <= TRACE_XOVR) {
        small  = 1;
        tbytes_ = sizeof(uint8);
    } else {
        small  = 0;
        tbytes_ = sizeof(uint16);
    }

    trg_ = new Target(db_, tspace, small);
    targSeqBuf_ = New_Read_Buffer(&db_);

    // Read in the first record
    nextRecord(prevRec_);
}

DazAlnProvider::~DazAlnProvider() {
    fclose(input_);
    Close_DB(&db_);
    free(targSeqBuf_-1);
    delete trg_;
}

bool DazAlnProvider::nextTarget(std::vector<dagcon::Alignment> &dest) {
    dest.clear();

    // constructor initializes the first prevRec_ struct
    trg_->firstRecord(prevRec_, popts_.properOvls);

    std::set<int> tfilt = popts_.targets;
    unsigned int filter = tfilt.size();
    bool skipTarget = false;
    while(covl_++ < novl_) {
        Record rec;
        nextRecord(rec);

        if (rec.ovl.aread != trg_->id || covl_ == novl_) {
            int tid = trg_->id + 1;
            if (filter == 0 || tfilt.find(tid) != tfilt.end()) {
                //trg_->getAlignments(dest, popts_.maxHits, popts_.sortCov);
                trg_->getBlasrAlignments(dest, popts_.maxHits, popts_.sortCov);
                if (dest.size() < popts_.minCov) {
                    dest.clear();
                    skipTarget = true;
                }
            } else {
                skipTarget = true;
            }

            if (skipTarget && covl_ != novl_) {
                trg_->firstRecord(rec, popts_.properOvls);
                skipTarget = false;
                continue;
            }
            // This will initialize the target object on the next call
            prevRec_ = rec;
            break;
        }
        trg_->addRecord(rec, popts_.properOvls);
    }

    return covl_ != novl_;
}

bool DazAlnProvider::nextTarget(std::string& targSeq, std::vector<dagcon::Alignment>& dest) {

    bool hasNext = nextTarget(dest);

    targSeq.resize(trg_->length);
    char* seq;
    seq = Load_Subread(&db_, trg_->id, 0, trg_->length, targSeqBuf_, 0);

    int i;
    for (i = 0; i < trg_->length; i++)
        targSeq[i] = ToU[(int)seq[i]];

    return hasNext;
}

void DazAlnProvider::nextRecord(Record& rec) {
    Read_Overlap(input_,&rec.ovl);
    int tmax = ((int)1.2*rec.ovl.path.tlen) + 100;
    rec.trace.resize(tmax,0);
    rec.ovl.path.trace = (void *) &rec.trace.front();
    Read_Trace(input_, &rec.ovl, tbytes_);
}

TargetHit::TargetHit() :
    ovlScore(0.0f),
    covScore(0.0f),
    aread(-1),
    bread(0),
    flags(0) {}

TargetHit::TargetHit(Record& rec) : TargetHit() {
    aread = rec.ovl.aread;
    bread = rec.ovl.bread;
    flags = rec.ovl.flags;
    add(rec);
}

std::ostream& operator<<(std::ostream& ostrm, TargetHit& hit) {
    ostrm << "target: " << (hit.aread+1) << " query: " << (hit.bread+1);
    ostrm << " flags: " << hit.flags << " tstart: " << hit.abeg();
    ostrm << " tend: " << hit.aend() << " gaps: ";
    ostrm << hit.records.size() << " ovlscore: " << hit.ovlScore;
    ostrm << " covScore: " << hit.covScore << std::endl;
    return ostrm;
}

bool TargetHit::belongs(Overlap& ovl) {
    return aread == ovl.aread &&
           bread == ovl.bread &&
           flags == ovl.flags;
}

void TargetHit::add(Record& rec) {
    if (records.size() == 0) {
        records.push_back(std::move(rec));
    } else {
        Path prev = records.back().ovl.path;
        int prevLen = prev.aepos - prev.abpos;

        Path curr = rec.ovl.path;
        int currLen = curr.aepos - curr.abpos;

        if (curr.abpos > prev.aepos) {
            records.push_back(std::move(rec));
        } else if (currLen > prevLen) {
            records.pop_back();
            records.push_back(std::move(rec));
        }
    }
}

void TargetHit::computeOvlScore(bool proper) {
    int ahlen = 0, bhlen = 0, diff = 0;

    // XXX: penalize for gaps between records?
    for (auto& rec: records) {
        Path p = rec.ovl.path;
        ahlen += p.aepos - p.abpos;
        bhlen += p.bepos - p.bbpos;
        diff += std::abs(ahlen - bhlen) + p.diffs;
    }

    ovlScore = (1 - diff/(float)ahlen) * ahlen;

    if (proper) {
        const Path& f = records.front().ovl.path;
        const Path& b = records.back().ovl.path;
        if (f.abpos != 0 && b.bbpos != 0)
            ovlScore = 0.0f;
        if (f.aepos != alen && b.bepos != blen)
            ovlScore = 0.0f;
    }
}

int TargetHit::abeg() {
    return records.front().ovl.path.abpos;
}

int TargetHit::aend() {
    return records.back().ovl.path.aepos;
}

int TargetHit::amin() {
    return records.front().ovl.path.abpos;
}

int TargetHit::amax() {
    return records.back().ovl.path.aepos;
}

int TargetHit::bmin() {
    return records.front().ovl.path.bbpos;
}

int TargetHit::bmax() {
    return records.back().ovl.path.bepos;
}

// Simplify unit testing, don't burden with malloc'd
// daligner structures.
Target::Target(): needsFree_(false) { }

Target::Target(HITS_DB& db, int tspace, int small) :
    db_(db),
    tspace_(tspace),
    small_(small),
    needsFree_(true) {

    work_ = New_Work_Data();
    abuffer_ = New_Read_Buffer(&db_);
    bbuffer_ = New_Read_Buffer(&db_);
}

Target::~Target() {
    if (needsFree_) {
        free(abuffer_-1);
        free(bbuffer_-1);
        Free_Work_Data(work_);
    }
}

void Target::firstRecord(Record& rec, bool proper) {
    id = rec.ovl.aread;
    length = db_.reads[id].rlen;

    hits.clear();
    coverage_.clear();

    if (coverage_.size() < (unsigned int) length)
        coverage_.resize(length);

    auto beg = coverage_.begin();
    std::for_each(beg, beg+length, [](unsigned int& x){x=0;});

    TargetHit hit(rec);
    hit.alen = length;
    hit.blen = db_.reads[rec.ovl.bread].rlen;
    hit.computeOvlScore(proper);
    hits.push_back(std::move(hit));
}

void Target::addRecord(Record& rec, bool proper) {
    // collapse into target hits, maintaining only the best hit from either
    // forward or reverse.
    if (hits.size() > 0) {
        TargetHit& prev = hits.back();

        // Add overlap to previous hit, instead of storing a new one
        if (prev.belongs(rec.ovl)) {
            prev.add(rec);
            prev.computeOvlScore(proper);
            return;
        }
    }

    TargetHit hit(rec);
    hit.alen = length;
    hit.blen = db_.reads[rec.ovl.bread].rlen;
    hit.computeOvlScore(proper);
    hits.push_back(std::move(hit));
}

void Target::sortHits(bool sortCov) {
    // sort descending based on ovl length x percent id
    std::sort(hits.begin(), hits.end(), cmpHitOvlScore);

    if (! sortCov) return;

    // Coverage based scoring
    for (auto& hit : hits) {
        for (auto const& rec : hit.records) {
            auto beg = coverage_.begin() + rec.ovl.path.abpos;
            auto end = coverage_.begin() + rec.ovl.path.aepos;
            std::for_each(beg, end, [](unsigned int& x){++x;});
            hit.covScore = std::accumulate(beg, end, 0.0, invertedSum);
        }
    }

    std::sort(hits.begin(), hits.end(), cmpHitCovScore);
}

void Target::getAlignments(std::vector<dagcon::Alignment> &alns, unsigned int max, bool sortCov) {
    sortHits(sortCov);
    auto hbeg = hits.begin();
    auto hend = hits.size() > max ? hbeg + max : hits.end();
    for (auto& hit = hbeg; hit != hend; ++hit) {
        //std::cerr << *hit;
        for (auto const& rec : hit->records) {
            Overlap ovl = rec.ovl;
            char* aseq, *bseq;
            int amin, amax;
            int bmin, bmax;
            Dalignment aln;
            aln.path = &(ovl.path);
            aln.alen  = length;
            aln.blen  = hit->blen;
            aln.flags = ovl.flags;

            if (small_)
                Decompress_TraceTo16(&ovl);

            amin = ovl.path.abpos - BORDER;
            if (amin < 0) amin = 0;
            amax = ovl.path.aepos + BORDER;
            if (amax > aln.alen) amax = aln.alen;
            if (COMP(aln.flags)) {
                bmin = (aln.blen-ovl.path.bepos) - BORDER;
                if (bmin < 0) bmin = 0;
                bmax = (aln.blen-ovl.path.bbpos) + BORDER;
                if (bmax > aln.blen) bmax = aln.blen;
            } else {
                bmin = ovl.path.bbpos - BORDER;
                if (bmin < 0) bmin = 0;
                bmax = ovl.path.bepos + BORDER;
                if (bmax > aln.blen) bmax = aln.blen;
            }

            // XXX: potential optimization, cache reads
            aseq = Load_Subread(&db_, ovl.aread, amin, amax, abuffer_, 0);
            bseq = Load_Subread(&db_, ovl.bread, bmin, bmax, bbuffer_, 0);

            aln.aseq = aseq - amin;
            if (COMP(aln.flags)) {
                Complement_Seq(bseq,bmax-bmin);
                aln.bseq = bseq - (aln.blen - bmax);
            } else
                aln.bseq = bseq - bmin;

            Compute_Trace_PTS(&aln, work_, tspace_, GREEDIEST);

            // initialize the dagcon alignment class
            dagcon::Alignment dest;
            std::stringstream tid, qid;
            tid << (ovl.aread+1);
            qid << (ovl.bread+1);
            dest.id = tid.str();
            dest.sid = qid.str();
            dest.tlen = aln.alen;

            dest.start = ovl.path.abpos+1;
            dest.end = ovl.path.aepos+1;

            decodeAlignment(&aln, dest);
            alns.push_back(dest);
        }
    }
}

void Target::getBlasrAlignments(std::vector<dagcon::Alignment> &alns, unsigned int max, bool sortCov) {
    SimpleAligner blasr;
    sortHits(sortCov);
    auto hbeg = hits.begin();
    auto hend = hits.size() > max ? hbeg + max : hits.end();
    for (auto& hit = hbeg; hit != hend; ++hit) {
        dagcon::Alignment dest;

        // target (a) id, query (b) id
        std::stringstream tid, qid;
        tid << (hit->aread+1);  
        qid << (hit->bread+1);
        dest.id = tid.str();
        dest.sid = qid.str();
        dest.tlen = hit->alen;

        // start/stop on target
        dest.start = hit->abeg() + 1;
        dest.end = hit->aend() + 1;
        
        // extract subsequences
        char* aseq, *bseq;
        int amin = hit->amin();
        int amax = hit->amax();
        int bmin = hit->bmin();
        int bmax = hit->bmax();
        aseq = Load_Subread(&db_, hit->aread, amin, amax, abuffer_, 0);
        bseq = Load_Subread(&db_, hit->bread, bmin, bmax, bbuffer_, 0);

        // if necessary, complement bread
        if (COMP(hit->flags))
            Complement_Seq(bseq,bmax-bmin);

        dest.tstr.resize(amax-amin);
        for (int i = 0; i < amax-amin; i++) 
            dest.tstr[i] = ToU[(int)aseq[i]];

        dest.qstr.resize(std::abs(bmax-bmin));
        for (int i = 0; i < bmax-bmin; i++) 
            dest.qstr[i] = ToU[(int)bseq[i]];

#ifdef DEBUG        
        if (COMP(hit->flags)) 
            std::cout << "-";
        else
            std::cout << "+";
        std::cout << std::endl;
        std::cout << dest << std::endl; 
#endif        

        // pass to SimpleAligner
        blasr(dest);
        alns.push_back(dest); 

#ifdef DEBUG
        std::cout << dest << std::endl; 
#endif        
    }
}

bool cmpHitOvlScore(const TargetHit& l, const TargetHit& r) {
    return l.ovlScore > r.ovlScore;
}

bool cmpHitCovScore(const TargetHit& l, const TargetHit& r) {
    return l.covScore > r.covScore;
}

float invertedSum(float x, unsigned int y) {
    return x + 1/(float)y;
}

void decodeAlignment(Dalignment* src, dagcon::Alignment& dest) {
    int i, j, tlen, c, p;
    char* a, *b; // pointers to the sequence
    int* trace = (int *) src->path->trace;
    a = src->aseq;
    b = src->bseq;
    tlen = src->path->tlen;
    i = src->path->abpos;
    j = src->path->bbpos;
    // XXX: slow. pre-allocate string length or work directly with
    // encoded strings.
    for (c = 0; c < tlen; c++) {
        if ((p = trace[c]) < 0) {
            p = -p;
            while (i != p) {
                dest.tstr += ToU[(int)a[i++]];
                dest.qstr += ToU[(int)b[j++]];
            }
            dest.tstr += ToU[7];
            dest.qstr += ToU[(int)b[j++]];
        } else {
            while (j != p) {
                dest.tstr += ToU[(int)a[i++]];
                dest.qstr += ToU[(int)b[j++]];
            }
            dest.tstr += ToU[(int)a[i++]];
            dest.qstr += ToU[7];
        }
    }
    p = src->path->aepos;
    while(i <= p) {
        dest.tstr += ToU[(int)a[i++]];
        dest.qstr += ToU[(int)b[j++]];
    }
}
