#!/bin/bash

echo "======================================"
echo "   IICPC TRADING PLATFORM INITIALIZING"
echo "======================================"

# 1. Kill any existing instances to prevent "Address already in use" errors
echo "[SYSTEM] Clearing ports 8000 and 8080..."
lsof -ti:8000 | xargs kill -9 2>/dev/null
lsof -ti:8080 | xargs kill -9 2>/dev/null

# 2. Boot the Dummy Exchange in the background
echo "[SYSTEM] Booting Dummy Exchange (Port 8080)..."
cd sandbox-api
python3 dummy_exchange.py &
EXCHANGE_PID=$!

# 3. Boot the FastAPI Hub in the background
echo "[SYSTEM] Booting FastAPI Hub (Port 8000)..."
python3 -m uvicorn main:app --port 8000 &
API_PID=$!

# 4. Give the servers 2 seconds to warm up
sleep 2

# 5. Open the Live Leaderboard Dashboard in your default browser
echo "[SYSTEM] Launching Live Dashboard..."
cd ../frontend
open index.html

echo "======================================"
echo " PLATFORM LIVE. AWAITING ATTACK FLEET."
echo " Press CTRL+C to safely shut down all services."
echo "======================================"

# Wait for user to press CTRL+C, then gracefully kill the background processes
trap "echo '\n[SYSTEM] Shutting down Platform...'; kill $EXCHANGE_PID $API_PID; exit" INT
wait