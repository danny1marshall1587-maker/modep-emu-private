import asyncio
import websockets

async def check():
    uri = "ws://127.0.0.1:8089"
    async with websockets.connect(uri, subprotocols=["ws-meters"]) as ws:
        for _ in range(3):
            msg = await ws.recv()
            print("LIVE_DATA:", msg)

if __name__ == "__main__":
    asyncio.run(check())
