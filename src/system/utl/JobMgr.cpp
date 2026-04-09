#include "utl/JobMgr.h"

namespace {
    int gJobIDCounter;
}

Job::Job() { mID = gJobIDCounter++; }

void JobMgr::Poll() {
    if (!mJobQueue.empty()) {
        if (mJobQueue.front()->IsFinished()) {
            Job *job = mJobQueue.front();
            mJobQueue.pop_front();
            mPreventStart = true;
            job->OnCompletion(mCallback);
            delete job;
            mPreventStart = false;
            if (!mJobQueue.empty()) {
                mJobQueue.front()->Start();
            }
        }
    }
}

JobMgr::JobMgr(Hmx::Object *o) : mCallback(o), mJobQueue(), mPreventStart(0) {}

JobMgr::~JobMgr() { CancelAllJobs(); }

void JobMgr::QueueJob(Job *job) {
    mJobQueue.push_back(job);
    if (mJobQueue.size() == 1 && !mPreventStart) {
        mJobQueue.front()->Start();
    }
}

bool JobMgr::HasJob(int id) {
    for (std::list<Job *>::const_iterator it = mJobQueue.begin(); it != mJobQueue.end();
         ++it) {
        if ((*it)->ID() == id)
            return true;
    }
    return false;
}

void JobMgr::CancelJob(int id) {
    std::list<Job *>::iterator it = mJobQueue.begin();
    while (it != mJobQueue.end()) {
        Job *job = *it;
        if (job->ID() == id) {
            int frontID = mJobQueue.front()->ID();
            it = mJobQueue.erase(it);
            bool oldstart = mPreventStart;
            mPreventStart = true;
            job->Cancel(mCallback);
            mPreventStart = oldstart;
            if (frontID == id && !oldstart && it != mJobQueue.end()) {
                (*it)->Start();
            }
            delete job;
            return;
        }
        ++it;
    }
    MILO_WARN("This job is not in the queue %i", id);
}

void JobMgr::CancelAllJobs() {
    std::list<Job *> dupeJobs = mJobQueue;
    mJobQueue.clear();
    for (std::list<Job *>::const_iterator it = dupeJobs.begin(); it != dupeJobs.end();
         ++it) {
        (*it)->Cancel(mCallback);
        delete *it;
    }
}
