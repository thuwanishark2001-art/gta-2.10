#pragma once

// Custom skin and vehicle models from the server's content (custommodels.cpp).
void CustomModels_InstallHooks();
// After CGame::Initialise: loads the game models custom ids are based on.
void CustomModels_OnGameInit();
