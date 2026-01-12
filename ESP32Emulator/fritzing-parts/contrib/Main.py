import os
import xml.etree.ElementTree as ET
import json

# Папка с Fritzing parts (contrib)
PARTS_DIR = "E:\\fritzing-parts\\contrib"
OUTPUT_DIR = "converted_parts"

os.makedirs(OUTPUT_DIR, exist_ok=True)

def parse_fzp(fzp_path):
    tree = ET.parse(fzp_path)
    root = tree.getroot()
    
    part_info = {}
    
    # Название
    title = root.find("title")
    part_info["title"] = title.text if title is not None else "Unnamed"
    
    # Описание
    desc = root.find("description")
    part_info["description"] = desc.text if desc is not None else ""
    
    # Список контактов
    part_info["connectors"] = []
    for connector in root.findall(".//connector"):
        c = {
            "id": connector.attrib.get("id"),
            "name": connector.attrib.get("name"),
            "type": connector.attrib.get("type"),
        }
        # Опционально: позиции контактов в разных views
        views = {}
        for view in ["breadboardView", "schematicView", "pcbView"]:
            v = connector.find(view)
            if v is not None:
                views[view] = v.attrib
        c["views"] = views
        part_info["connectors"].append(c)
    
    # SVG файлы
    part_folder = os.path.dirname(fzp_path)
    svgs = {}
    for view in ["breadboard", "schematic", "pcb"]:
        svg_path = os.path.join(part_folder, f"{view}.svg")
        if os.path.exists(svg_path):
            svgs[view] = svg_path
    part_info["svgs"] = svgs
    
    return part_info

# Парсим все .fzp в contrib
all_parts = []
for root_dir, dirs, files in os.walk(PARTS_DIR):
    for file in files:
        if file.endswith(".fzp"):
            fzp_path = os.path.join(root_dir, file)
            part_data = parse_fzp(fzp_path)
            all_parts.append(part_data)

# Сохраняем JSON
with open(os.path.join(OUTPUT_DIR, "contrib_parts.json"), "w", encoding="utf-8") as f:
    json.dump(all_parts, f, indent=2, ensure_ascii=False)

print(f"Конвертировано {len(all_parts)} частей в {OUTPUT_DIR}/contrib_parts.json")
