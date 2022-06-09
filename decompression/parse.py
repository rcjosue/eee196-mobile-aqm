import json

file = open('generated.json')

data = json.load(file)

print(data['values']['pm10'])

for i in data['values']:
    print(i)

file.close()