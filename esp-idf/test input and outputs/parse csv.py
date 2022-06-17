import csv
count = 0
with open('Raw-Input.csv', newline='') as csvfile:
     #read = csv.reader(csvfile, delimiter=',', quotechar='|')
     with open('rawInputformatted.txt', 'w') as f:
          for row in csvfile:
               if (count==0 ):
                    f.write('char tm_arr[16][100] = {')
                    count +=1                    
               elif (count==1 ):
                    f.write('float hum_arr[32] = {')
                    count +=1
               elif (count==2 ):
                    f.write('float pm_10_arr[32]= {')
                    count +=1
               elif (count==3 ):
                    f.write('float pm_25_arr[32] = {')
                    count +=1
               elif (count==4 ):
                    f.write('float temp_arr[32] = {')
                    count +=1
               elif (count==4 ):
                    count +=1
               else :
                    count=0
               f.write(row + '};')
               f.write('\n')
 

