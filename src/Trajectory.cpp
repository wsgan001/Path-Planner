/*
 * Trajectory.cpp
 *
 *  Created on: Oct 16, 2017
 *      Author: jyarde
 */

#include "Trajectory.h"

/**
 * Created by the expand() method
 */
PathNode::PathNode(VehiclePose start, vector<vector<double>> heuristic) :
	move(FSM::KE),
	heuristic(heuristic),
	pos(start),
	path(),
	cost(0),
	turns()
	{}

/**
 * start is either a x,y coordinates of starting point or Entry object
 */
PathNode::PathNode(FSM& move, PathNode start) :
	move(move),
	pos(start.calc_move(move)),
	heuristic(start.heuristic),
	path(start.path),
	turns(start.turns)
{
	this->path.push_back(this->pos);
	this->turns.push_back(move);
	this->cost = (start.cost + move_costs[(int)move] + this->calc_g());
}

PathNode::~PathNode(){

}

/**
 * Is this move allowed?
 */
bool PathNode::valid_move(VehiclePose pos, vector<vector<int>> grid) {
	int x = pos.grid_x;
	int y = pos.grid_y;
	bool x_in_range = x >= 0  && x < grid.size();
	bool y_in_range = y >= 0  && y < grid[0].size();
	return x_in_range && y_in_range && grid[x][y] == 0;
}

/**
 * Adjust cost for heuristic
 **/
double PathNode::calc_g() {
	if (this->heuristic.empty())
		return 0.0;
	return this->heuristic[this->pos.grid_x][this->pos.grid_y];
}

VehiclePose PathNode::calc_move(FSM move) {
	VehiclePose start = this->pos;
	return PathNode::calc_move(move, start);
}

VehiclePose PathNode::calc_move(FSM move, VehiclePose start) {
	int x = start.grid_x + this->delta[(int)move][0];
	int y = start.grid_y + this->delta[(int)move][1];
	VehiclePose pose;
	pose.grid_x = x;
	pose.grid_y = y;
	return pose;
}

/**
 * Generate a list of Entry objects that represent the possible new positions from current
 */
void PathNode::expand(vector<vector<int>>grid, queue<PathNode> &que) {
	cout << "Expanding: " << this->pos << endl;
	vector<PathNode> expansion;
	double min_cost = 9999999999;
	for (int m = 0; m < this->allowed_moves.size(); m++) {
		VehiclePose p = this->calc_move(this->allowed_moves[m]);
		if (this->valid_move(p, grid)) {
			PathNode new_entry(allowed_moves[m], *this);
			if (new_entry.cost == min_cost) {
				expansion.push_back(new_entry);
			}
			else if (new_entry.cost < min_cost) {
				/**
				 * clear the list and set the new min cost
				 */
				min_cost = new_entry.cost;
				cout << "\tNew Min Cost at: " << this->pos << " Cost: " << min_cost << " " << new_entry.pos;
				expansion.clear();
				expansion.push_back(new_entry);
			}
		}
	}
	cout << "\nExpansion: \n" << expansion << endl;
	/**
	 * Now add them to the queue
	 **/
	for (auto e : expansion) {
		que.push(e);
	}
}

/**
 * A* SEARCH
 */
AStar::AStar() {
}

AStar::~AStar() {
}

bool AStar::arrived(VehiclePose goal, VehiclePose current) {
	return goal.grid_x == current.grid_x && goal.grid_y == current.grid_y;
}

/**
 * Step cost heuristic
 */
void AStar::init_heuristic(vector<vector<int>> grid, VehiclePose goal) {
	int r = grid.size();
	int c = grid[0].size();
	cout << "x width: " << r << "y height: " << c << "\n";
	this->grid = grid;
	this->heuristic = vector<vector<double>>(r, vector<double>(c, 0.0));
	for(int i=0; i < r; i++) {
		for(int j=0; j < c; j++) {
			this->heuristic[i][j] = ( abs(goal.grid_x - i) + abs(goal.grid_y - j) );
		}
	}
}

void AStar::print_world() {
	printf("World Map: \n");
	for (auto r : this->grid) {
		for (auto c : r) {
			printf("%4i, ", c);
		}
		printf("\n");
	}
	printf("\n");
}

void AStar::print_heuristic() {
	printf("Heuristic function: \n");
	for (auto r : this->heuristic) {
		for (auto c : r) {
			printf("%6.2f, ", c);
		}
		printf("\n");
	}
	printf("\n");
}

void AStar::search(VehiclePose init, VehiclePose goal) {
	init_heuristic(grid, goal);
	print_world();
	print_heuristic();

	cout << "Starting from: " << init << " destination: " << goal << endl;
	//Pass the heuristic for the first Entry
	PathNode start(init, this->heuristic);
	queue<PathNode> que;
	que.push(start);
	PathNode e = start;
	while  (!que.empty()) {
		e = que.front();
		que.pop();
		if (this->arrived(goal, e.pos)) {
			cout << "Best Route cost: " << e.cost << endl;
			cout << e.path;
			vector<vector<string>> route = e.map_path(grid);
			printf("Route:\n");
			for (auto& m : route) {
				cout << "\t" << m << endl;
			}
			break;
		}
		if (grid[e.pos.grid_x][e.pos.grid_y] == 0) {
			//Only expand un-expanded or drivable locations
			e.expand(grid, que);
			//Update expanded list
			grid[e.pos.grid_x][e.pos.grid_y] = 2;
		}
	}
	print_world();
}


/**
 * TRAJECTORY PLANNER
 */

TrajectoryPlanner::TrajectoryPlanner(vector<double> x, vector<double> y, vector<double> s,
									 vector<double> dx, vector<double> dy) :
											 map_x(x),
											 map_y(y),
											 map_s(s),
											 map_dx(dx),
											 map_dy(dy),
											 closestWaypoint(0),
											 grid_start_s(0),
											 target_lane(1){
	this->track_length = map_x.size();
}

TrajectoryPlanner::TrajectoryPlanner() :
										 map_x(),
										 map_y(),
										 map_s(),
										 map_dx(),
										 map_dy(),
										 closestWaypoint(0),
										 grid_start_s(0),
										 track_length(0),
										 target_lane(1),
										 t(1.0){
	T = MatrixXd(3, 3);
	T << pow(t, 3), pow(t, 4), pow(t, 5),
		3*pow(t, 2), 4*pow(t, 3), 5*pow(t, 4),
		6*t, 12*pow(t, 2), 20*pow(t, 3);
	T_inverse = T.inverse();
}


TrajectoryPlanner::~TrajectoryPlanner(){}

/**
 * Create a grid over the next N waypoints starting at an the vehicle.
 * interpolate a waypoint for the vehicle from the closest waypoint.
 * Place all detected vehicles on the grid fully covering all cells they touch.
 * Cells are square with side equal to lane width.
 * N set to cover sensor range from vehicle.
 * Must include closest waypoint behind the vehicle.
 */
void TrajectoryPlanner::state_update(VehiclePose vehicle, vector<vector<double>> sensor_fusion, int closestWaypoint, double max_s) {
	this->location = vehicle;
	this->closestWaypoint = closestWaypoint;
	int grid_width = ROAD_MAX / CELL_SIDE;	//width in cells
	/**
	 * Start the grid at SENSOR_RANGE waypoints behind the closest waypoint to the egocar
	 * Then count forward 2*SENSOR_RANGE waypoints and that is the bounds of the grid
	 */
	int start = track_length + (closestWaypoint - SENSOR_RANGE) % track_length;

	if (closestWaypoint > 5)
		start = closestWaypoint - 5;		//Map at least 5 cells behind the ego car
	int grid_length = (int) 2*SENSOR_RANGE*(max_s/181)/4;		//2*SENSOR_RANGE is in waypoints which are max_s/181 apart;
	map_grid = vector<vector<int>>(grid_width, vector<int>(grid_length, 0));	//init map with all 0's
	this->grid_start_s = map_s[start];

	/**
	 * Now map all traffic onto the grid
	 */
	//traffic.clear();
	//Structure: [ 0, 1, 2,  3,  4, 5, 6]
	//Structure: [id, x, y, vx, vy, s, d]
  	for (auto sf : (vector<vector<double>>)sensor_fusion) {
  		//TODO: Account for vehicles changing lanes (straddling cells)
  		//TODO: Make an assumption about vehicle dimensions to do this
  		//Vehicle is in range (in the grid of concern)
  		double distance = sf[5] - grid_start_s;
  		if (distance <= 0)
  			distance += max_s;

		int grid_x = 0;
		if (sf[6] > LANE_WIDTH) {
			if (sf[6] <= 2*LANE_WIDTH)
				grid_x = 1;
			else
				grid_x = 2;
		}

		int grid_y = (int) distance/CELL_SIDE;
		//place vehicle in the grid
		map_grid[grid_x][grid_y] = 1;
  	}

	/**
	 * Map the ego car
	 */
	double distance = vehicle.s - grid_start_s;
	if (distance <= 0)
		distance += max_s;
	int grid_x = 0;
	if (vehicle.d > LANE_WIDTH) {
		if (vehicle.d <= 2*LANE_WIDTH)
			grid_x = 1;
		else
			grid_x = 2;
	}
	int grid_y = (int) distance/CELL_SIDE;
	map_grid[grid_x][grid_y] = 4;


	cout << "Map Grid: \n";
  	for (auto row : map_grid)
  		cout << row << endl;
}


/**
 * Generate target d based on requested state. Adjust requested speed based on
 * traffic.
 */
inline void apply_requested_state(
		FSM state,
		VehiclePose ego_car,
		vector<vector<VehiclePose>> sorted_traffic,
		Trajectory& new_trajectory,
		int& target_lane,
		double& df,
		double& sf_dot,
		double& target_s) {

	sf_dot = SPEED_LIMIT_MPS; //Always unless conditions dictate otherwise
	/**
	 * Update final state position. Conditions for update:
	 * 1. Time horizon is length of the trajectory (50 points or 1 second)
	 * 2. Target speed: speed limit or speed of overtaking traffic (behind) in the target lane
	 * 3. Acceleration: 0
	 * 4. Sf: the distance of travel over the time horizon (equivalent to speed for 1 second)
	 */
	if (state == FSM::CL) {
		if (ego_car.lane > 1) {
			//Change to left lane if possible
			new_trajectory.state_possible = true;
			/**
			 * Check for close traffic in the next lane since we can only travel
			 * as far as traffic will allow
			 */
			target_lane = ego_car.lane - 1;
			if (sorted_traffic[target_lane-1].size() > 0) {
				for (VehiclePose current_car : sorted_traffic[target_lane-1]) {
					if (current_car.leading) {
						if (current_car.v < SPEED_LIMIT_MPS){
							sf_dot = current_car.v * cos(current_car.yaw);
						}
						//Traffic is sorted by leading and distance so quit loop after first
						//'leading' hit
						break;
					}
				}
			}
			//Update motion parameters
			df = lane_center(target_lane);
		} else
			new_trajectory.state_possible = false;
	} else

	if (state == FSM::CR) {
		if (ego_car.lane < 3) {
			//Change to right lane if possible
			new_trajectory.state_possible = true;
			/**
			 * Check for close traffic in the next lane since we can only travel
			 * as far as traffic will allow
			 */
			target_lane = ego_car.lane + 1;
			if (sorted_traffic[target_lane-1].size() > 0) {
				for (VehiclePose current_car : sorted_traffic[target_lane-1]) {
					if (current_car.leading) {
						if (current_car.v < SPEED_LIMIT_MPS){
							sf_dot = current_car.v * cos(current_car.yaw);
						}
						//Traffic is sorted by leading and distance so quit loop after first
						//'leading' hit
						break;
					}
				}
			}
			//Update motion parameters
			df = lane_center(target_lane);
		} else
			new_trajectory.state_possible = false;
	} else
	if (state == FSM::PL) {
		/**
		 * To Keep Lane: develop a trajectory that keeps you behind cars ahead
		 * So match speed of cars ahead if they are lower than the speed limit
		 */
		target_lane = ego_car.lane;
		if (sorted_traffic[target_lane-1].size() > 0) {
			for (VehiclePose current_car : sorted_traffic[target_lane-1]) {
				if (current_car.leading) {
					if (current_car.v < ego_car.v){
						sf_dot = current_car.v;
					}
					//Traffic is sorted by leading and distance so quit loop after first
					//'leading' hit
					break;
				}
			}
		}
		//Update motion parameters
		df = lane_center(target_lane);
	}
	//TODO: REMOVE. FOR TESTING ONLY
	target_lane = ego_car.lane;
	//df = lane_center(target_lane);
	df = 6.0;
	target_s = 30;
}

/**
 * Generates trajectories to execute the instructions of the BehaviourPlanner
 * Trajectories are for the next 50 timesteps (1 second of travel).
 *
 * Assumes that the BehaviorPlanner sends a snapshot of the world as it will be at the start of the
 * next timestep of the trajectory.
 *
 * Operations:
 * 		1. Translate traffic predictions into a working grid.
 * 		2. Generate a destination cell based on requested state and 50 timesteps ahead
 * 			(e.g KeepLane: 50 timesteps ahead in the same lane, TurnLeft: 50 timesteps ahead in the left lane
 * 		3. Hybrid A* search to the destination (5th order polynomial for each expansion
 */
Trajectory TrajectoryPlanner::plan_trajectory(
		FSM state,
		VehiclePose ego_car,
		vector<vector<VehiclePose>> sorted_traffic,
		int remainder,
		double end_s,
		double end_d) {


	/**
	 * If state and conditions are same as previous, generate a new or reuse old trajectory with extension:
	 * 		- Traffic ahead is beyond endpoint of old trajectory
	 * 		- State requested is same as previous
	 * 		- Traffic ahead is at or above the speed limit
	 *
	 * If conditions are different, always generate a new trajectory.
	 */
	trajectory.target_state = state;

	/**
	 * Assess how to obey the state request and change df and sf_dot accordingly
	 */
	double target_v; // Target speed. Either speed limit of speed of car ahead
	double target_d; // Target lateral location (equates to lane)
	double target_s; // Limit to trajectory by distance (otherwise limit by time)
	apply_requested_state(state, ego_car, sorted_traffic, trajectory, target_lane,
			target_d, target_v, target_s);

	cout << "Previous State: car s, \t\tend_s, \t\tcar v, \t\tend v, \t\tremainder" << endl;
	cout << "Previous State: " << ego_car.s << ", \t" << end_s << ", \t" << ego_car.v << ", \t" << trajectory.sf_dot << ", \t" << remainder << endl;

	/**
	 * Initial State 's'
	 */
	VectorXd a 		= VectorXd(6);
	double s 		= ego_car.s;
	double s_dot 	= ego_car.v;
	double s_dotdot	= MAX_ACCELERATION;
	if (remainder > 0) {
		s	  = end_s;
		s_dot = trajectory.sf_dot;	//Starting speed for next iteration
	}

	/**
	 * Final State 's'
	 */
	double sf		= s + target_s;
	double sf_dot	= min( target_v, sqrt(s_dot*s_dot + 2*s_dotdot*target_s) );	//final speed
	double sf_dotdot= MAX_ACCELERATION;											//final acceleration
	double v_diff 	= sf_dot - s_dot;
	double dist_a 	= v_diff*v_diff/(2*s_dotdot);								//distance travelled during acceleration

	/**
	 * Setup the acceleration trajectory
	 */
	if ( !in_range(s_dot, target_v, 0.04) ) {
		/**
		 * solve for quintic trajectory coefficients
		 */
		t = fabs(v_diff) / s_dotdot;
		T << pow(t, 3), pow(t, 4), pow(t, 5),
			3*pow(t, 2), 4*pow(t, 3), 5*pow(t, 4),
			6*t, 12*pow(t, 2), 20*pow(t, 3);
		T_inverse = T.inverse();

		VectorXd Sf = VectorXd(3);
		Sf << sf - (s + s_dot*t + s_dotdot * pow(t,2)/2),
				sf_dot - (s_dot + s_dotdot * t),
				sf_dotdot - s_dotdot;
		VectorXd A = T_inverse * Sf;
		a << s, s_dot, s_dotdot/2, A[0], A[1], A[2];
	} else {
		// Already at required speed
		a << s, s_dot, 0, 0, 0, 0;
	}

	/**
	 * Initial State 'd'
	 */
	VectorXd b 		= VectorXd(6);
	double d 		= ego_car.d;
	double d_dot 	= 0.0;
	double d_dotdot	= 0.5;
	if (remainder > 0)
		d = end_d;

	/**
	 * Final State 'd'
	 */
	double df		= target_d;	//center of lane chosen below
	double df_dotdot= 0.0; 		//final acceleration (any adjustment done in time horizon 't')
	double df_dot	= 0.0; 		//Adjustment over so no lateral movement necessary

	if (state != FSM::KE) {
		/**
		 * Setup coefficients
		 */
		t = sqrt( 2 * fabs(df - d) / d_dotdot );
		T << pow(t, 3), pow(t, 4), pow(t, 5),
			3*pow(t, 2), 4*pow(t, 3), 5*pow(t, 4),
			6*t, 12*pow(t, 2), 20*pow(t, 3);
		T_inverse = T.inverse();

		VectorXd Df = VectorXd(3);
		Df << df - (d + d_dot*t + d_dotdot * pow(t,2)/2),
				df_dot - (d_dot + d_dotdot * t),
				df_dotdot - d_dotdot;
		VectorXd B = T_inverse * Df;
		b << d, d_dot, d_dotdot/2, B[0], B[1], B[2];
	} else
		b << df, 0, 0, 0, 0, 0;

	/**
	 * DEBUG:
	cout << "\na coefficients: \n" << a << endl;
	cout << "\nb coefficients: \n" << b << endl;
	**/

	/**
	 * Generate the trajectory.
	 */
	VectorXd DT(6);
	VectorXd a_s(6);
	double st = s;
	double dt = df;
	a_s << a[1], 2*a[2], 3*a[3], 4*a[4], 5*a[5], 0;
	trajectory.s.clear();
	trajectory.d.clear();

	for (int i = 1; i < HORIZON-remainder; i++) {
		double ti = i * INTERVAL;
		DT << 1, ti, pow(ti,2), pow(ti,3), pow(ti,4), pow(ti,5);

		if ( in_range(trajectory.sf_dot, target_v, 0.04) ) {
			// Cruising speed
			st = st + trajectory.sf_dot * INTERVAL;
		} else {
			st = a.transpose() * DT;
			trajectory.sf_dot = a_s.transpose() * DT;
		}
		trajectory.s.push_back( st );

		if (state != FSM::KE) {
			if (!in_range(dt, df, 0.01)) {
				dt = b.transpose() * DT;
			}
		}
		trajectory.d.push_back( dt );
	}

	/**
	 * DEBUG:
	cout << "_____________________>" << endl;
	cout << "Generated Trajectory:" << endl;
	cout << "s: \n" << trajectory.s << endl;
	cout << "d: \n" << trajectory.d << endl;
	cout << "_____________________|" << endl;
	**/
	return trajectory;
}

/**
 * plot a trajectory for the target speed and path
 */
Trajectory TrajectoryPlanner::generate_trajectory(
		FSM state,
		VehiclePose ego_car,
		vector<vector<VehiclePose>> sorted_traffic) {
	Trajectory trajectory;
	//TODO:
	return trajectory;
}

