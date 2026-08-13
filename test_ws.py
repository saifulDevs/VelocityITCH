import asyncio
import websockets
import json

async def run():
    async with websockets.connect("ws://127.0.0.1:8000/ws/market") as ws:
        # Wait for messages
        count = 0
        while count < 3000:
            msg = await ws.recv()
            data = json.loads(msg)
            if data.get("msg") == "w" and data.get("ob") == 508:
                print("WS UPDATE OB 508:", msg)
            count += 1
            if count % 500 == 0:
                print(f"Received {count} messages...")

asyncio.run(run())
