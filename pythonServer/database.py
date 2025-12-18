from sensible import *
import psycopg2
from psycopg2.extras import RealDictCursor
from typing import Dict

#while True:
try:
    conn = psycopg2.connect(host='localhost', 
                            database=database, 
                            user=user, 
                            password=password,
                            cursor_factory=RealDictCursor)
    cursor = conn.cursor()
    print("La conexion a la base de datos fue exitosa")
    #break
except Exception as error:
    print(f"Falló la conexion a la base de datos. Error: {error}")
    #time.sleep(3)


class AuthenticationError(Exception):
    """Excepción personalizada para errores de autenticación"""
    pass
                                                        # MAC address
async def authenticate_user_with_device( username: str, client_id: str, db_connection_string: str) -> Dict:
    """
    Autentica un usuario y verifica que el dispositivo esté asociado.
    
    Args:
        username: Nombre de usuario
        client_id: Dirección MAC del dispositivo
        db_connection_string: String de conexión a PostgreSQL
        
    Returns:
        Dict con datos del usuario y dispositivo para verificación
        
    Raises:
        AuthenticationError: Si el usuario no existe, no está activo, 
                           o el dispositivo no está asociado
    """
    try:
        with psycopg2.connect(db_connection_string) as conn:
            with conn.cursor() as cur:
                # Consulta que une users y devices
                query = """
                    SELECT 
                        u.user_id,
                        u.username,
                        u.password,
                        u.is_active,
                        d.device_mac,
                        d.device_id
                    FROM users u
                    INNER JOIN devices d ON u.user_id = d.user_id
                    WHERE u.username = %s AND d.device_mac = %s
                """
                
                cur.execute(query, (username, client_id))
                result = cur.fetchone()
                
                # Usuario no existe o dispositivo no asociado
                if result is None:
                    raise AuthenticationError(
                        "Usuario no encontrado o dispositivo no asociado a este usuario"
                    )
                
                user_id, username, password_hash, is_active, device_mac, device_id = result
                
                # Verificar si el usuario está activo
                if not is_active:
                    raise AuthenticationError("Usuario inactivo")
                
                # Retornar datos necesarios para la verificación
                return {
                    'user_id': user_id,
                    'username': username,
                    'password_hash': password_hash,
                    'is_active': is_active,
                    'device_mac': device_mac,
                    'device_id': device_id
                }
                
    except psycopg2.Error as e:
        # Error de base de datos
        raise Exception(f"Error de base de datos: {str(e)}")
