// ============================================================
// File: DialogueManager.h
// Responsibility: Load linear dialogue scripts from JSON data files.
//
// Owns:
//   No global state. Each load returns a value object that DialogueState owns.
//
// Lifetime:
//   Created on demand by DialogueState.
//   Destroyed when DialogueState exits.
//
// Important:
//   - Display strings are resolved through LocalizationManager.
//   - Scripts remain linear in V1; the data shape leaves room for choices later.
// ============================================================
#pragma once

#include <string>
#include <vector>

struct DialogueLine
{
    std::string speakerId;
    std::string speakerName;
    std::string text;
};

struct DialogueScript
{
    std::string id;
    std::string completionFlag;
    bool skippable = false;
    std::vector<DialogueLine> lines;
};

class DialogueManager
{
public:
    bool LoadScript(const std::string& path, DialogueScript& outScript) const;
};
