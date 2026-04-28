---
name: testnet-setup
description: 'Qubic Core Lite Testnet Deployment and Operations. Use for: deploying contract changes to testnet server, rebuilding the Qubic binary, starting/stopping the node, switching main/aux mode with qubic-cli, adding contract tests, troubleshooting node startup, managing the operator key, checking node logs. Server: 135.181.160.185, Branch: testnet-setup.'
argument-hint: 'deploy | rebuild | restart | togglemain | test | logs'
---

# Qubic Core Lite – Testnet Setup

## Infrastructure

| Parameter | Value |
|-----------|-------|
| **Server** | `root@135.181.160.185` (Hetzner, 936 GB RAID) |
| **Branch** | `testnet-setup` on `MZoxx/core-lite` |
| **Git remote** | `mzoxx` → `https://github.com/MZoxx/core-lite.git` |
| **Binary** | `/root/core-lite/build/src/Qubic` |
| **Build dir** | `/root/core-lite/build/` |
| **Node log** | `/root/core-lite/node.log` |
| **qubic-cli** | `/root/qubic/scripts/qubic-cli` |
| **Peer port** | `31841` |
| **HTTP port** | `41841` |
| **Epoch** | 208 (TESTNET) |

## Operator Identity

| Parameter | Value |
|-----------|-------|
| **Operator seed** | `enfdotlfasfasjtovpbaoyfjngjtxmddexmtqveuyrcgjpfcunhtrgn` |
| **Operator pubkey** | `WIYVZEQZEOBACHHGYWMLMVXJWVMCUQXJJUKJNSKSTEWJXYTHAJHTNRAENCUH` |
| **Operator alias** | `qMine.io` |
| **`private_settings.h`** | `OPERATOR` must match the pubkey above |

> **Critical**: `operatorPublicKey` in the node is derived **only** from `OPERATOR` in `src/private_settings.h` — NOT from `--operator-seed`. Both must always be consistent.

## Workflows

### 1. Deploy Contract Changes

After editing `src/contracts/*.h` locally:

```bash
# 1. Commit and push to MZoxx fork
git add <files> && git commit -m "fix: <description>"
git push mzoxx testnet-setup

# 2. Pull on server and rebuild binary
ssh root@135.181.160.185 'cd /root/core-lite && git pull mzoxx testnet-setup'
ssh root@135.181.160.185 'cd /root/core-lite/build && cmake --build . --target Qubic -j4 2>&1 | tail -5'

# 3. Restart node
ssh root@135.181.160.185 'pkill -f "build/src/Qubic" 2>/dev/null || true'
ssh root@135.181.160.185 "nohup /root/core-lite/build/src/Qubic --operator-seed enfdotlfasfasjtovpbaoyfjngjtxmddexmtqveuyrcgjpfcunhtrgn --operator-alias qMine.io > /root/core-lite/node.log 2>&1 &"
```

### 2. Run Contract Tests (on server)

```bash
# Build test binary
ssh root@135.181.160.185 'cd /root/core-lite/build && cmake --build . --target qubic_core_tests -j4 2>&1 | tail -5'

# Run specific contract tests (e.g. WolfPack)
ssh root@135.181.160.185 "cd /root/core-lite/build/test && ./qubic_core_tests --gtest_filter='*WolfPack*' 2>&1"

# Run all tests
ssh root@135.181.160.185 'cd /root/core-lite/build/test && ./qubic_core_tests 2>&1 | tail -20'
```

### 3. Switch Node to MAIN Mode

Run **after** the node has fully started (wait ~10 s):

```bash
ssh root@135.181.160.185 'sleep 8 && /root/qubic/scripts/qubic-cli \
  -nodeip 127.0.0.1 -nodeport 31841 \
  -seed enfdotlfasfasjtovpbaoyfjngjtxmddexmtqveuyrcgjpfcunhtrgn \
  -togglemainaux MAIN MAIN 2>&1'
```

Expected output: `Successfully set MAINAUX flag`

> `-togglemainaux <MODE_0> <MODE_1>`: bit 0 = MODE_0, bit 1 = MODE_1. Both `MAIN` = flag 3 (fully main).

### 4. Check Node Status

```bash
# Live log tail
ssh root@135.181.160.185 'tail -30 /root/core-lite/node.log'

# Process running?
ssh root@135.181.160.185 'ps aux | grep Qubic | grep -v grep'

# Open ports
ssh root@135.181.160.185 'ss -tlnp | grep Qubic'

# HTTP tick info
ssh root@135.181.160.185 'curl -s http://localhost:8000/v2/tick-info'
```

### 5. Change Operator Key

If operator seed changes, update **both**:

1. `src/private_settings.h` → `static std::string OPERATOR = "<NEW_PUBKEY>";`
2. Start command → `--operator-seed <NEW_SEED>`

Then rebuild and restart (see Workflow 1).

## Contract Development Rules

See `src/contracts/WP.h` for reference implementation. Key constraints from the Qubic contract spec:

- **`qpi.numberOfPossessedShares()`** to check share balance (not `acquireShares` for checks)
- **`qpi.releaseShares()`** fee must be `WOLFPACK_QX_TRANSFER_FEE` (not `0`)
- **`PRE_ACQUIRE_SHARES()`** system procedure must be implemented to reject incoming share transfers to the contract
- **`while(true)` loops are forbidden** — use `for (locals.idx = map.nextElementIndex(NULL_INDEX); locals.idx != NULL_INDEX; locals.idx = map.nextElementIndex(locals.idx))`
- **`INITIALIZE()`** must NOT call `qpi.originator()` for admin setup — use bootstrap via `NULL_ID` check in setter instead
- `SetAdmin` bootstrap: only allow setting admin if current `adminAddress == NULL_ID`

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Failed set MAINAUX flag` | `OPERATOR` pubkey ≠ operator seed | Sync `private_settings.h` `OPERATOR` with seed pubkey, rebuild |
| `Failed to connect 127.0.0.1` | Node not yet listening on port 31841 | Wait ~10 s after start |
| `Error opening file in load spectrum.208!` | No snapshot files (fresh start) | Non-fatal, node starts with empty state |
| `Error opening file in load system!` | No saved system file | Non-fatal on first start |
| Disk full | Old contract/docker data | `docker system prune -a --volumes -f` + `rm -rf /root/qubic-testnet/` |
| Build fails after contract edit | Syntax/spec violation in `.h` | Run tests first: `cmake --build . --target qubic_core_tests` |

## Node Start Command (reference)

```bash
nohup /root/core-lite/build/src/Qubic \
  --operator-seed enfdotlfasfasjtovpbaoyfjngjtxmddexmtqveuyrcgjpfcunhtrgn \
  --operator-alias qMine.io \
  > /root/core-lite/node.log 2>&1 &
```
