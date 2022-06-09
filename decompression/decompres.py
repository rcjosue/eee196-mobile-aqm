import csv
import numpy as np
from scipy.fft import dct, idct
from math import log10, sqrt
import datetime
import time

def psnr(original, reconstructed):
    # my_sum = 0
    # for i in range(16):
    #     my_sum += (original[i]-reconstructed[i]) ** 2
    # my_mean = my_sum/16
    # rmse = sqrt(my_mean)

    mse = np.mean((original-reconstructed) ** 2)
    rmse = sqrt(mse)
    nrmse = rmse/np.mean(original)
    print("NRMSE:", nrmse)

    if(rmse == 0):  # MSE is zero means no noise is present in the signal .
                  # Therefore PSNR have no importance.
        return 100
    psnr = 20 * log10((max(original)-min(original)) / rmse)
    print("PSNR:", psnr)
    return psnr

with open('Zach2 Timeseries.csv') as csv_file:
    with open('Raw Input.csv', 'a', newline='') as raw_input_file:
        csv_reader= csv.reader(csv_file, delimiter=';')
        line_count = 0
        ts = []
        hum = []
        pm10 = []
        pm25 = []
        temp = []
        next(csv_reader)
        for row in csv_reader:
            if line_count == 16:

                writer = csv.writer(raw_input_file)
                writer.writerows([ts, pm10, pm25, temp, hum, ''])

                line_count = 0

                ts = []
                hum = []
                pm10 = []
                pm25 = []
                temp = []

            (ts_tmp, hum_tmp, pm10_tmp, pm25_tmp, temp_tmp) = row

            ts_format = datetime.datetime.strptime(ts_tmp, "%Y-%m-%d %H:%M:%S")
            ts_unix = datetime.datetime.timestamp(ts_format)
            ts.append(ts_unix)
            hum.append(float(hum_tmp))
            pm10.append(float(pm10_tmp))
            pm25.append(float(pm25_tmp))
            temp.append(float(temp_tmp))
            line_count+=1

        writer = csv.writer(raw_input_file)
        writer.writerows([ts, pm10, pm25, temp, hum, ''])

        line_count = 0

        ts = []
        hum = []
        pm10 = []
        pm25 = []
        temp = []


        

    

# compressed = np.array([20.2000, -2.5467, 1.5047, 4.5842, -1.7507, 0, 1.2206, 1.8646,
# 1.1000, 0.3014, 0, 0.7963, 0.3463, 0, 0, 0])

# reconstructed = idct(compressed, norm='ortho')

# print(reconstructed)

# original = np.array([9,8,8,9,7,7,9,10,8,7,8,8,9,8,7,7])
# reconstructed = np.array([8.964626,7.807805,8.132063,9.159341,6.966051,7.133396,9.015735,
# 9.72294,
# 8.02706,
# 6.984266,
# 7.866604,
# 8.283949,
# 9.090659,
# 7.867937,
# 7.192195,
# 6.785374])

# psnr(original, reconstructed)



#di kailangan
# ts_str = ', '.join(str(n) for n in ts)
                # pm10_str = ', '.join(str(n) for n in pm10)
                # pm25_str = ', '.join(str(n) for n in pm25)
                # temp_str = ', '.join(str(n) for n in temp)
                # hum_str = ', '.join(str(n) for n in hum)

                # print("ts:", ts_str)
                # print("hum:", pm10_str)
                # print("pm10:", pm25_str)
                # print("pm25:", temp_str)
                # print("temp:", hum_str)
            
# writer.writerows([ts_str, pm10_str, pm25_str, temp_str, hum_str, ''])
                # raw_input_file.write(ts_str)
                # raw_input_file.write('\n')
                # raw_input_file.write(pm10_str)
                # raw_input_file.write('\n')
                # raw_input_file.write(pm25_str)
                # raw_input_file.write('\n')
                # raw_input_file.write(temp_str)
                # raw_input_file.write('\n')
                # raw_input_file.write(hum_str)
                # raw_input_file.write('\n')
                # raw_input_file.write('\n')