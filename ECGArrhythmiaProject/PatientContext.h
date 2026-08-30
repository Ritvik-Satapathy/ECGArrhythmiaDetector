#ifndef PATIENT_CONTEXT_H
#define PATIENT_CONTEXT_H

#include <string>

enum class ActivityState {
    RelaxedRest = 1,
    AwakeRestNotRelaxed = 2,
    ExercisedWithin15Minutes = 3,
    Exercised15To60MinutesAgo = 4,
    Unknown = 5
};

enum class SportType {
    None = 0,
    Endurance = 1,
    RunningIntermittent = 2,
    StrengthPower = 3,
    Other = 4
};

struct PatientContext {
    int age = 30;
    ActivityState activityState = ActivityState::Unknown;
    bool playsSports = false;
    SportType sportType = SportType::None;
    int trainingDaysPerWeek = 0;
};

inline std::string activityStateName(ActivityState state) {
    switch (state) {
        case ActivityState::RelaxedRest: return "relaxed/resting";
        case ActivityState::AwakeRestNotRelaxed: return "resting but not fully relaxed";
        case ActivityState::ExercisedWithin15Minutes: return "exercised within the last 15 minutes";
        case ActivityState::Exercised15To60MinutesAgo: return "exercised 15-60 minutes ago";
        default: return "unknown";
    }
}

inline std::string sportTypeName(SportType sport) {
    switch (sport) {
        case SportType::Endurance: return "endurance";
        case SportType::RunningIntermittent: return "running/intermittent";
        case SportType::StrengthPower: return "strength/power";
        case SportType::Other: return "other";
        default: return "none";
    }
}

#endif
