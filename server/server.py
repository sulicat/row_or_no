from flask import Flask, request, send_from_directory, jsonify
from flask_cors import CORS
import json
import os

app = Flask(__name__, static_folder="website")
CORS(app)  # This enables CORS for all routes and origins by default


# In-memory store (could also be a class member)
data_store = None
file_path = "lake_info.json"

def load_json(path):
    """Read a JSON file and store the result into memory."""
    global data_store
    with open(path, "r", encoding="utf-8") as f:
        data_store = json.load(f)
    return data_store

def save_json(path):
    """Take the in-memory JSON data and write it back to file."""
    global data_store
    if data_store is None:
        raise ValueError("No data loaded into memory.")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data_store, f, indent=4)



# Serve index.html
@app.route("/")
def home():
    return send_from_directory(app.static_folder, "index.html")

# Example POST endpoint
@app.route("/api/new_sensor_data", methods=["POST"])
def handle_data():
    payload = request.json  # or request.form for form data
    print("Received:", payload)

    if "lake_name" in payload and "temprature" in payload:
        lake_name = payload["lake_name"]
        data_store[lake_name] = {"temprature": payload["temprature"]}
        save_json(file_path)

    # Do something with the payload...
    return jsonify({"status": "ok", "received": payload})

@app.route("/api/get_lake_temp", methods=["POST"])
def status():
    payload = request.json

    out = {"ok": True}

    if "lake_name" in payload:
        lake_name = payload["lake_name"]
        if lake_name in data_store:
            out["data"] = data_store[lake_name]
            return out

    return {"ok": False, "message": "no lake name or no lake data"}

@app.route("/api/get_all_lakes", methods=["POST"])
def get_all_lakes():
    payload = request.json

    return {"ok": True, "data": data_store}




if __name__ == "__main__":
    load_json(file_path)
    use_ssl = os.getenv("USE_SSL", "1") == "1"
    
    if use_ssl:
        cert = os.getenv("SSL_CERT")
        key = os.getenv("SSL_KEY")

        if not cert or not key:
            raise RuntimeError("SSL enabled but SSL_CERT or SSL_KEY not set.")

        ssl_context = (cert, key)
        print(f"Running with SSL on port 5000")
    else:
        ssl_context = None
        print(f"Running WITHOUT SSL on port 5000")


    app.run(host="0.0.0.0", port=5000, debug=True, ssl_context=ssl_context)
