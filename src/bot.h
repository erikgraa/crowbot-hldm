#pragma once

void Bot_Init();                 // register cvars and the "crowbot" server command
void Bot_ServerActivate();
void Bot_ServerDeactivate();
void Bot_StartFrame();
void Bot_ClientDisconnect(edict_t *ent);
bool Bot_ClientCommand(edict_t *ent);  // true if the command was ours
bool Bot_FakeCmdActive();
int  Bot_FakeArgc();
const char *Bot_FakeArgv(int i);
const char *Bot_FakeArgs();
