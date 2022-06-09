# UWB distance contains the distance information measured by an ultra-wideband positioning system,
# such as Pozyx or NXP Rddrone.

uint64 	timestamp		# time since system start (microseconds)
uint32 	time_uwb_ms		# Time of UWB device in ms
uint32 	counter			# Number of Ranges since last Start of Ranging
uint32 	sessionid		# UWB SessionID
uint32 	time_offset		# Time between Ranging Rounds in ms
uint16 	status			# status feedback #

uint16[12] anchor_distance	# distance in cm to each UWB Anchor (UWB Device wich takes part in Ranging)
bool[12] nlos			# Visual line of sight yes/no

#Angle of arrival, Angle in Degree -70..+70
float32[12] aoa_azimuth_dev	# Angle of arrival of first incomming RX msg
float32[12] aoa_altitude_dev	# Angle of arrival of first incomming RX msg
# GPS:
#GPS position of target_pos of the UWB system
float32[4] gps_data #Latitude, Longitude, Altitude, and Yaw Offset to True North  #
float32[3] position		# Position of the Landing point in relation to the Drone (x,y,z in Meters NED)