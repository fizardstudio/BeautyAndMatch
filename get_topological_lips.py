import json

# Outer lips
lipTopIdx = [61, 185, 40, 39, 37, 0, 267, 269, 270, 409, 291]
lipBotIdx = [291, 375, 321, 405, 314, 17, 84, 181, 91, 146, 61]
outer_lips = set(lipTopIdx + lipBotIdx)

# Inner lips
innerLipTopIdx = [78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308]
innerLipBotIdx = [308, 324, 318, 402, 317, 14, 87, 178, 88, 95, 78]
inner_lips = set(innerLipTopIdx + innerLipBotIdx)

# Read triangles
with open("cpp/FizgravityMeshIndices.h", "r") as f:
    content = f.read()

start = content.find("{") + 1
end = content.find("}")
array_str = content[start:end]
indices = [int(x.strip()) for x in array_str.split(",") if x.strip().isdigit()]

# Build adjacency list
adj = {}
for i in range(468):
    adj[i] = set()

for i in range(0, len(indices), 3):
    t = indices[i:i+3]
    if len(t) == 3:
        adj[t[0]].add(t[1])
        adj[t[0]].add(t[2])
        adj[t[1]].add(t[0])
        adj[t[1]].add(t[2])
        adj[t[2]].add(t[0])
        adj[t[2]].add(t[1])

# BFS
visited = set(inner_lips)
queue = list(inner_lips)
lips_region = set(inner_lips).union(outer_lips)

while queue:
    curr = queue.pop(0)
    if curr in outer_lips:
        continue # Don't expand beyond outer lips
        
    for neighbor in adj[curr]:
        if neighbor not in visited:
            visited.add(neighbor)
            lips_region.add(neighbor)
            queue.append(neighbor)

print(f"Topological Lips Size: {len(lips_region)}")
print("LIPS:", sorted(list(lips_region)))
