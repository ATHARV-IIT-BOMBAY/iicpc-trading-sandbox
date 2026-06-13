from fastapi import FastAPI, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
import shutil
import os
import subprocess

app = FastAPI()

# Allow our local HTML file to fetch data without security blocks
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPLOAD_DIR = os.path.join(BASE_DIR, "sandbox-api", "contestant_submissions")
DOCKERFILE_PATH = os.path.join(BASE_DIR, "infrastructure", "Dockerfile.sandbox")
os.makedirs(UPLOAD_DIR, exist_ok=True)

# --- NEW: Leaderboard Memory ---
latest_telemetry = {
    "status": "Awaiting Data...",
    "p50": 0,
    "p90": 0,
    "p99": 0
}

@app.post("/submit-telemetry/")
async def submit_telemetry(data: dict):
    global latest_telemetry
    latest_telemetry = data
    latest_telemetry["status"] = "Live"
    return {"status": "Leaderboard Updated"}

@app.get("/leaderboard-data/")
async def get_leaderboard():
    return latest_telemetry

# (Keep your existing upload_engine route here)
@app.post("/upload-engine/")
async def upload_engine(file: UploadFile = File(...)):
    file_location = os.path.join(UPLOAD_DIR, "engine.cpp")
    with open(file_location, "wb+") as file_object:
        shutil.copyfileobj(file.file, file_object)
        
    try:
        # 1. Build the isolated image
        subprocess.run(["docker", "build", "-t", "contestant-sandbox", "-f", DOCKERFILE_PATH, BASE_DIR], check=True)
        
        # --- UPGRADE: CPU Pinning & Memory Limits ---
        # --cpuset-cpus="0": Forces the bot to run exclusively on CPU Core 0
        # --memory="512m": Hard limits the container to 512 Megabytes of RAM
        # --network="none": (Optional but hardcore) Cuts off internet access so the bot can't cheat or leak data
        
        # 2. Run the locked-down container and capture the bot's trades!
        result = subprocess.run([
            "docker", "run", 
            "--cpuset-cpus=0", 
            "--memory=512m", 
            "-p", "9000:9000",   # <--- THE NEW UPGRADE: Open port 9000
            "contestant-sandbox"
        ], capture_output=True, text=True, check=True)
        
        # 3. Print the execution logs directly to your terminal
        print("\n=== CONTESTANT ENGINE EXECUTION LOGS ===")
        print(result.stdout)
        print("========================================\n")

        return {"info": "Engine received and executed (CPU 0 Pinned)", "docker_status": "Success"}
    except subprocess.CalledProcessError as e:
        print(f"\n[ERROR] Sandbox Failed: {e.stderr}")
        return {"info": "Engine received", "docker_status": "Execution Failed"}