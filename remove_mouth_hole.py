import re

inner_lips = {78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308, 324, 318, 402, 317, 14, 87, 178, 88, 95}

with open("cpp/FizgravityMeshIndices.h", "r") as f:
    content = f.read()

# Find the array content
start = content.find("{") + 1
end = content.find("}")
array_str = content[start:end]

# Extract numbers
indices = [int(x.strip()) for x in array_str.split(",") if x.strip().isdigit()]

# Process triangles
new_indices = []
removed_count = 0
for i in range(0, len(indices), 3):
    t = indices[i:i+3]
    if len(t) == 3:
        if t[0] in inner_lips and t[1] in inner_lips and t[2] in inner_lips:
            # This is a mouth hole triangle! Remove it.
            removed_count += 1
            print(f"Removed mouth hole triangle: {t}")
            continue
        new_indices.append(t[0])
        new_indices.append(t[1])
        new_indices.append(t[2])

print(f"Total triangles removed: {removed_count}")

# Rebuild the file
new_array_str = ", ".join(str(x) for x in new_indices)
new_content = content[:start] + "\n    " + new_array_str + "\n" + content[end:]

with open("cpp/FizgravityMeshIndices.h", "w") as f:
    f.write(new_content)

print("Updated FizgravityMeshIndices.h")
