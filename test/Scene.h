#ifndef __SCENE_H__
#define __SCENE_H__

#include "task_helper.h"
#include "task_scheduler.h"

enum SCENE_STATE
{
    SCENE_STATUS_NORMAL = 0,
    SCENE_STATUS_RUNNING = 1,
};

class Obj_Human
{
public:
    Obj_Human() : score(0) {}
    int GetGuildId() { return 0; }
    int AddScore() {score++; return score;}
    int GetScore() {return score;}
    void SetScore(int s) {score = s;}
private:
    int score;
};

class Scene : public CTaskScheduler 
{
public:
    Scene() : CTaskScheduler("Scene") {};
    void                                Tick() {ConsumeTask();}
    int                                 SceneID() { return 0; }
    Obj_Human*                          GetHumanObjInSceneByGUID(int guid) { return new Obj_Human(); }
};

#endif