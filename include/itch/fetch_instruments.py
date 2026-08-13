import asyncio
import websockets
import json

async def fetch():
    instruments = []
    try:
        async with websockets.connect("wss://uat-itchservice.skytrade.us/market") as ws:
            print("Connected. Listening for messages...")
            # We'll listen for a few seconds to collect all 'w' (Watchlist/Orderbook) messages
            for _ in range(3000):
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=5.0)
                    data = json.loads(msg)
                    if data.get("msg") == "w":
                        instruments.append(data)
                except asyncio.TimeoutError:
                    break
    except Exception as e:
        print(f"Error: {e}")
    
    # generate C++ code
    with open("real_instruments.hpp", "w") as f:
        f.write("#pragma once\n")
        f.write("#include <vector>\n")
        f.write("#include \"types.hpp\"\n")
        f.write("#include \"event_generator.hpp\"\n")
        f.write("namespace itch {\n")
        f.write("inline std::vector<Instrument> get_real_instruments() {\n")
        f.write("    return {\n")
        
        count = 0
        seen_ob = set()
        for d in instruments:
            # We need orderbook_id, symbol, long_name, isin, base_price
            ob = d.get("ob", 0)
            if ob in seen_ob: continue
            seen_ob.add(ob)
            
            symbol = d.get("t", "")
            name = d.get("fn", "")
            isin = d.get("ic", "")
            if not isin: isin = "BD0000000000"
            price = d.get("lp", 0.0)
            if price == 0.0: price = d.get("r", 10.0) # ref_price
            base_price = int(price * 100) # cents
            
            f.write(f'        {{{ob}, "{symbol}", "{name}", "{isin}", {base_price}}},\n')
            count += 1
            
        f.write("    };\n")
        f.write("}\n")
        f.write("} // namespace itch\n")
        
    print(f"Saved {count} instruments to real_instruments.hpp")

asyncio.run(fetch())
