with open("roadmap_yos_x86_64.txt", "r") as f:
    text = f.read()

text = text.replace("Etape 94 :", "x Etape 94 :")

with open("roadmap_yos_x86_64.txt", "w") as f:
    f.write(text)

print("[+] Etape 94 cochee dans la roadmap!")
