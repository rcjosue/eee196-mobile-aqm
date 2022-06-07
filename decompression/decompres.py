import csv
import numpy as np
from scipy.fft import dct, idct


with open('Zach Timeseries.csv') as csv_file:
    csv_reader= csv.reader(csv_file, delimiter=';')
    line_count = 0
    ts = []
    hum = []
    pm10 = []
    pm25 = []
    temp = []
    next(csv_reader)
    for row in csv_reader:
        (ts_tmp, hum_tmp, pm10_tmp, pm25_tmp, temp_tmp) = row
        ts.append(ts_tmp)
        hum.append(float(hum_tmp))
        pm10.append(float(pm10_tmp))
        pm25.append(float(pm25_tmp))
        temp.append(float(temp_tmp))

    print("ts:", ts)
    print("hum:", hum)
    print("pm10:", pm10)
    print("pm25:", pm25)
    print("temp:", temp)

    

compressed = np.array([20.2000, -2.5467, 1.5047, 4.5842, -1.7507, 0, 1.2206, 1.8646,
1.1000, 0.3014, 0, 0.7963, 0.3463, 0, 0, 0])

reconstructed = idct(compressed, norm='ortho')

print(reconstructed)

