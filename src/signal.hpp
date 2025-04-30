#pragma once

void badsigHandler(int signum) noexcept;
void sigintHandler(int signum) noexcept;
void sigtermHandler(int signum) noexcept;
void setupSignalHandlers(void);
