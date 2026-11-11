# RECOVERY CHAT HISTORY — Quantum GammaWave

## Accessibility note
This agent (Buffy, cloud worktree) cannot enumerate or read other Freebuff chats/sessions. Only the repository history, branches, commits, and files in this worktree are accessible. No chat transcripts were retrieved. Claims about prior chat content are therefore limited to what is verifiable in the git history and repository files.

## Sessions inferred from git history (not from chat logs)
- The commit messages and CHANGELOG.md constitute the only recoverable "session" record. They document decisions, measurements, and rejected approaches (e.g., buffer moves to POOL reverted, voice pool branches discarded).
- Key historical documents: CHANGELOG.md, docs/VALIDATION_MATRIX.md, docs/VOICE_POOL.md, docs/OPTIMIZATION_BASELINE.md, docs/TRACKS_VOICES_3.4.md, docs/IDENTITY.md, FM1_INSTALLATION_SAFETY_AUDIT.md.

## Inaccessible
- Freebuff chat transcripts, archived sessions, and agent outputs from parallel chats are not accessible from this worktree. If the platform exposes an export, it should be run by the account owner and provided to the agent.

## Recovered decisions (from commits/docs)
- Identity chosen as FM-1_965 (docs/IDENTITY.md, tools/check_identity.py).
- 12-voice pool retained; 10-voice build still one flag away (FELUCCA_VOICES=10).
- Buffer moves to POOL reverted (below 8 KiB floor).
- Karplus-Strong only as offline wave generator (tools/gen_waves.py).