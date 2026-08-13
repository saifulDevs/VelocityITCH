from pymongo import MongoClient
client = MongoClient("mongodb://127.0.0.1:27017/")
db = client["itch_db"]
doc = db.market.find_one({"sec_code": "SONARBAINS"})
if doc:
    print("trading_state:", repr(doc.get("trading_state")), "ref_price:", doc.get("ref_price"))
else:
    print("Not found")
