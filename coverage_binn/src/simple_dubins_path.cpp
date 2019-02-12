#include <nav_msgs/Path.h>
#include <simple_dubins_path/simple_dubins_path.h>
#include <tf/tf.h>

#include <cmath>

const double epsilon = std::numeric_limits<double>::epsilon();

SimpleDubinsPath::SimpleDubinsPath() {
  ros::NodeHandle nh;
  ros::NodeHandle private_nh("~");

  m_turningRadius = private_nh.param("turning_radius", 1.5);
  m_pathResolution = private_nh.param("path_resolution", 0.05);

  m_pathPub = nh.advertise<nav_msgs::Path>("simple_dubins_path", 1000);
}

SimpleDubinsPath::~SimpleDubinsPath() {}

SimpleDubinsPath::Dir SimpleDubinsPath::turningDirection(double x_q, double y_q,
                                                         double theta_q,
                                                         double x_n,
                                                         double y_n) {
  /*
   * Create a new reference frame rotated "theta_q" about the x-axis. See which
   * side of this new x-axis the target is on. North-West-Up coordinate system.
   */
  Dir turningDirection = Right;
  if (-(x_n - x_q) * sin(theta_q) + (y_n - y_q) * cos(theta_q) > 0) {
    turningDirection = Left;
  }
  return turningDirection;
}

void SimpleDubinsPath::turningCenter(double x_q, double y_q, double theta_q,
                                     double x_n, double y_n, double& x_cr,
                                     double& y_cr) {
  /*
   * The turning circle closest to the target is the correct one.
   */
  double x_cr1 = x_q + sin(theta_q) * m_turningRadius;
  double y_cr1 = y_q - cos(theta_q) * m_turningRadius;
  double x_cr2 = x_q - sin(theta_q) * m_turningRadius;
  double y_cr2 = y_q + cos(theta_q) * m_turningRadius;

  x_cr = x_cr2;
  y_cr = y_cr2;
  if (std::pow(x_n - x_cr1, 2) + std::pow(y_n - y_cr1, 2) <
      std::pow(x_n - x_cr2, 2) + std::pow(y_n - y_cr2, 2)) {
    x_cr = x_cr1;
    y_cr = y_cr1;
  }
}

void SimpleDubinsPath::tangentLine(double x_n, double y_n, double x_cr,
                                   double y_cr, double& beta1, double& beta2) {
  /*
   * Find angle of tangent lines to the turning circle by solving for beta:
   * ((x_cr − x_n) sin(beta) + (y_n − y_cr) cos(beta))^2 = m_turningRadius^2
   */
  double a = (x_cr - x_n);
  double b = (y_n - y_cr);
  beta1 = 0;
  beta2 = 0;
  if (std::abs(b + m_turningRadius) < epsilon) {
    beta1 = 2 * atan((a - std::sqrt((a * a + b * b -
                                     m_turningRadius * m_turningRadius))) /
                     (b - m_turningRadius));
    beta2 = 2 * atan((a + std::sqrt((a * a + b * b -
                                     m_turningRadius * m_turningRadius))) /
                     (b - m_turningRadius));
  } else {
    beta1 = 2 * atan((a + std::sqrt((a * a + b * b -
                                     m_turningRadius * m_turningRadius))) /
                     (b + m_turningRadius));
    beta2 = 2 * atan((a - std::sqrt((a * a + b * b -
                                     m_turningRadius * m_turningRadius))) /
                     (b + m_turningRadius));
  }
  // Force beta in [0, pi)
  if (beta1 < 0) {
    beta1 = beta1 + M_PI;
  }
  if (beta2 < 0) {
    beta2 = beta2 + M_PI;
  }
}

void SimpleDubinsPath::tangentPoint(double x_q, double y_q, double x_n,
                                    double y_n, double x_cr, double y_cr,
                                    double beta1, double beta2, Dir dir,
                                    double& x_lc, double& y_lc) {
  /*
   * Find first tangent point on the turning circle along the direction of
   * rotation.
   *
   * Circle-line intersection with circle in origin:
   * http://mathworld.wolfram.com/Circle-LineIntersection.html
   */
  double x2 = x_n - x_cr;  // move point towards origin
  double y2 = y_n - y_cr;  // move point towards origin

  // Tangent point 1: (x_lc1, y_lc1)
  double x1 = (x_n + cos(beta1)) - x_cr;  // move point towards origin
  double y1 = (y_n + sin(beta1)) - y_cr;  // move point towards origin
  double dx = x2 - x1;
  double dy = y2 - y1;
  double dr = std::sqrt(dx * dx + dy * dy);
  double D = x1 * y2 - x2 * y1;

  double x_lc1 = D * dy / (dr * dr);
  double y_lc1 = -D * dx / (dr * dr);
  x_lc1 = x_lc1 + x_cr;  // move back from origin
  y_lc1 = y_lc1 + y_cr;  // move back from origin

  // Tangent point 2: (x_lc2, y_lc2)
  x1 = (x_n + cos(beta2)) - x_cr;  // move point towards origin
  y1 = (y_n + sin(beta2)) - y_cr;  // move point towards origin
  dx = x2 - x1;
  dy = y2 - y1;
  dr = std::sqrt(dx * dx + dy * dy);
  D = x1 * y2 - x2 * y1;

  double x_lc2 = D * dy / (dr * dr);
  double y_lc2 = -D * dx / (dr * dr);
  x_lc2 = x_lc2 + x_cr;  // move back from origin
  y_lc2 = y_lc2 + y_cr;  // move back from origin

  // Define vectors from the center to the start and tangent points
  double v_start[2] = {x_q - x_cr, y_q - y_cr};
  double v_lc1[2] = {x_lc1 - x_cr, y_lc1 - y_cr};
  double v_lc2[2] = {x_lc2 - x_cr, y_lc2 - y_cr};

  // Angle difference to tangent point 1
  x1 = v_start[0];
  y1 = v_start[1];
  x2 = v_lc1[0];
  y2 = v_lc1[1];
  double dot = x1 * x2 + y1 * y2;  // dot product
  double det = x1 * y2 - y1 * x2;  // determinant
  double angle1 = std::atan2(det, dot);
  if (angle1 < 0) angle1 += 2 * M_PI;  // wrap to [0, 2*pi]

  // Angle difference to tangent point 2
  x2 = v_lc2[0];
  y2 = v_lc2[1];
  dot = x1 * x2 + y1 * y2;  // dot product
  det = x1 * y2 - y1 * x2;  // determinant
  double angle2 = std::atan2(det, dot);
  if (angle2 < 0) angle2 += 2 * M_PI;  // wrap to [0, 2*pi]

  // Find the first tangent point encountered along the direction of rotation
  x_lc = x_lc2;
  y_lc = y_lc2;
  double angle = angle2;
  if (dir == Left) {
    if (angle1 < angle2) {
      x_lc = x_lc1;
      y_lc = y_lc1;
      angle = angle1;
    }
  } else if (dir == Right) {
    if (angle1 > angle2) {
      x_lc = x_lc1;
      y_lc = y_lc1;
      angle = angle1;
    }
  }
}

void SimpleDubinsPath::generatePath(double x_q, double y_q, double x_n,
                                    double y_n, double x_cr, double y_cr,
                                    double x_lc, double y_lc, Dir dir,
                                    const geometry_msgs::PoseStamped& goal,
                                    nav_msgs::Path& path) {
  /*
   * Generate points on the path and creates a ROS message.
   */
  path.header.stamp = ros::Time::now();
  path.header.frame_id = "map";
  path.poses.clear();

  // Find circle segment to follow
  double startAngle = std::atan2(y_q - y_cr, x_q - x_cr);
  if (startAngle < 0) startAngle += 2 * M_PI;  // wrap to [0, 2*pi]
  double stopAngle = std::atan2(y_lc - y_cr, x_lc - x_cr);
  if (stopAngle < 0) stopAngle += 2 * M_PI;  // wrap to [0, 2*pi]

  if (dir == Left && stopAngle < startAngle) {
    stopAngle += 2 * M_PI;
  } else if (dir == Right && stopAngle > startAngle) {
    stopAngle -= 2 * M_PI;
  }

  // Generate points on circle segment
  double angleIncrement = m_pathResolution / m_turningRadius;
  for (double i = startAngle; std::abs(i - stopAngle) > 2 * angleIncrement;
       i += dir * angleIncrement) {
    geometry_msgs::PoseStamped point;
    point.header.stamp = ros::Time::now();
    point.header.frame_id = "map";
    point.pose.position.x = x_cr + cos(i) * m_turningRadius;
    point.pose.position.y = y_cr + sin(i) * m_turningRadius;
    path.poses.push_back(point);
  }

  // Line description
  double dx = x_n - x_lc;
  double dy = y_n - y_lc;
  double dx_norm = dx / std::sqrt(dx * dx + dy * dy);
  double dy_norm = dy / std::sqrt(dx * dx + dy * dy);

  // Generate points on straight line segment
  for (double i = 0;
       std::abs(i * m_pathResolution * dx_norm - dx) > 2 * m_pathResolution;
       ++i) {
    geometry_msgs::PoseStamped point;
    point.header.stamp = ros::Time::now();
    point.header.frame_id = "map";
    point.pose.position.x = x_lc + i * m_pathResolution * dx_norm;
    point.pose.position.y = y_lc + i * m_pathResolution * dy_norm;
    path.poses.push_back(point);
  }

  path.poses.push_back(goal);
}

bool SimpleDubinsPath::makePath(const geometry_msgs::PoseStamped& start,
                                const geometry_msgs::PoseStamped& goal,
                                nav_msgs::Path& path) {
  // Initial configuration
  double x_q = start.pose.position.x;
  double y_q = start.pose.position.y;
  double theta_q = tf::getYaw(start.pose.orientation);

  // Target position
  double x_n = goal.pose.position.x;
  double y_n = goal.pose.position.y;

  Dir dir = turningDirection(x_q, y_q, theta_q, x_n, y_n);

  // Find the center of the turning circle
  double x_cr, y_cr;
  turningCenter(x_q, y_q, theta_q, x_n, y_n, x_cr, y_cr);

  // Perform checks
  if (m_turningRadius >
      std::sqrt(std::pow(x_q - x_n, 2) + std::pow(y_q - y_n, 2)) / 2) {
    ROS_WARN(
        "The desired turning radius is larger than half the length "
        "between the waypoint.");
  }
  if (std::sqrt(std::pow(x_n - x_cr, 2) + std::pow(y_n - y_cr, 2)) <
      m_turningRadius) {
    ROS_ERROR("Target not reachable with simple Dubin's path.");
    return false;
  }

  // Find angle of tangent line from target to turning circle
  double beta1, beta2;
  tangentLine(x_n, y_n, x_cr, y_cr, beta1, beta2);

  // Find tangent point
  double x_lc, y_lc;
  tangentPoint(x_q, y_q, x_n, y_n, x_cr, y_cr, beta1, beta2, dir, x_lc, y_lc);

  // Generate path
  generatePath(x_q, y_q, x_n, y_n, x_cr, y_cr, x_lc, y_lc, dir, goal, path);

  return true;
}

bool SimpleDubinsPath::getTargetHeading(double x_q, double y_q, double theta_q,
                                        double x_n, double y_n,
                                        double& yawTarget) {
  Dir dir = turningDirection(x_q, y_q, theta_q, x_n, y_n);

  // Find the center of the turning circle
  double x_cr, y_cr;
  turningCenter(x_q, y_q, theta_q, x_n, y_n, x_cr, y_cr);

  // Is target reachable?
  if (std::sqrt(std::pow(x_n - x_cr, 2) + std::pow(y_n - y_cr, 2)) <
      m_turningRadius) {
    return false;
  }

  // Find angle of tangent line from target to turning circle
  double beta1, beta2;
  tangentLine(x_n, y_n, x_cr, y_cr, beta1, beta2);

  // Find tangent point
  double x_lc, y_lc;
  tangentPoint(x_q, y_q, x_n, y_n, x_cr, y_cr, beta1, beta2, dir, x_lc, y_lc);

  yawTarget = std::atan2(y_n - y_lc, x_n - x_lc);
  return true;
}