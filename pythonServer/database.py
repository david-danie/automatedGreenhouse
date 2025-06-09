import psycopg2
from psycopg2.extras import RealDictCursor

#while True:
try:
    conn = psycopg2.connect(host='localhost', 
                            database='cropDevices', 
                            user='postgres', 
                            password='pass_987654321',
                            cursor_factory=RealDictCursor)
    cursor = conn.cursor()
    print("La conexion a la base de datos fue exitosa")
    #break
except Exception as error:
    print(f"Falló la conexion a la base de datos. Error: {error}")
    #time.sleep(3)