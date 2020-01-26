#include "rotation.h"
#include "SO3Filters.h"

// For debug
#include <opencv2/highgui.hpp>
#include <opencv2/plot.hpp>


namespace virtualgimbal
{

// Template specialization for Eigen::Quaterniond
template <>
int StampedDeque<Eigen::Quaterniond>::get(ros::Time time, Eigen::Quaterniond &q)
{
    constexpr bool verbose = false; // FOR DEBUG
    // std::cout << "Specialized" << std::endl;
    if(data.empty())
    {
        return DequeStatus::EMPTY;
    }
    auto it = std::find_if(data.begin(), data.end(), [&](std::pair<ros::Time, Eigen::Quaterniond> x) { return time < x.first; });
    if (data.begin() == it)
    {
        q = data.front().second;

        if (verbose)
        {
            ROS_INFO("Input time : %d.%d", time.sec, time.nsec);
            ROS_INFO("Size of data:%lu", data.size());

            ROS_INFO("Data time begin:  %d.%d", data.front().first.sec, data.front().first.nsec);
            ROS_INFO("Data time end: %d.%d", data.back().first.sec, data.back().first.nsec);
            for (int i = 0; i < data.size(); ++i)
            {
                printf("%d:%d.%d\r\n", i, data[i].first.sec, data[i].first.nsec);
            }
        }

        return DequeStatus::TIME_STAMP_IS_EARLIER_THAN_FRONT;
    }
    else if (data.end() == it)
    {
        q = data.back().second;

        if (verbose)
        {
            ROS_INFO("Input time : %d.%d", time.sec, time.nsec);
            ROS_INFO("Size of data:%lu", data.size());

            ROS_INFO("Data time begin:  %d.%d", data.front().first.sec, data.back().first.nsec);
            ROS_INFO("Data time end: %d.%d", data.front().first.sec, data.back().first.nsec);
            for (int i = 0; i < data.size(); ++i)
            {
                printf("%d:%d.%d\r\n", i, data[i].first.sec, data[i].first.nsec);
            }
        }

        return DequeStatus::TIME_STAMP_IS_LATER_THAN_BACK;
    }
    else
    {
        // Slerp, Interpolation of quaternion.
        auto pit = it - 1;
        double a = (time - pit->first).toSec() / (it->first - pit->first).toSec();
        q = pit->second.slerp(a, it->second);
        return DequeStatus::GOOD;
    }
};
template <>
Eigen::Vector3d StampedDeque<Eigen::Vector3d>::get(ros::Time time)
{
    Eigen::Vector3d v;
    if(data.empty())
    {
        throw;
    }
    auto it = std::find_if(data.begin(), data.end(), [&](std::pair<ros::Time, Eigen::Vector3d> x) { return time < x.first; });
    if (data.end() == it)
    {
        throw;
    }
    else
    {
        // Slerp, Interpolation of quaternion.
        auto pit = it - 1;
        double a = (time - pit->first).toSec() / (it->first - pit->first).toSec();
        return it->second * a + pit->second * (1.0-a);
    }
};

    template <>
    void StampedDeque<Eigen::Quaternion<double>>::print_least_squares_method(const ros::Time &begin, const ros::Time &end)
    {
        // beginを探す
        // endを探す
        // beginとendの間で最小二乗法
        // beginとendの間にデータが存在することは他の関数で保証させる
        auto begin_el   = std::find_if(data.begin(),data.end(),[&begin](std::pair<ros::Time, Eigen::Quaterniond> x) { return begin < x.first; });
        auto end_el     = std::find_if(data.begin(),data.end(),[&end](std::pair<ros::Time, Eigen::Quaterniond> x) { return end < x.first; });
        int num = std::distance(begin_el,end_el);

        // if(data.size() > 100)
        // {
        //     auto el = data.end() - 100;
        Eigen::VectorXd time(num);
        Eigen::VectorXd angle(num);
        // auto el = begin_el;
        for(int i=0;i < num;++i)
        {
            time(i)  = ((begin_el+i)->first - (end_el)->first).toSec();
            angle(i) = Quaternion2Vector((begin_el+i)->second * end_el->second.conjugate())[0];
        }
        Eigen::VectorXd coeff = least_squares_method(time,angle,2);
        printf("Coeff:%f %f %f\r\n",coeff(0),coeff(1),coeff(2));
        for(int i=0;i<num;++i)
        {
            std::cout << time(i) << "," << angle(i) << "," << coeff(0) + time(i) * coeff(1) + pow(time(i),2.0) * coeff(2) << std::endl;
        }
        std::cout << std::flush;
        // }
    }

    template <>
    Eigen::Quaterniond StampedDeque<Eigen::Quaternion<double>>::get_correction_quaternion_using_least_squares_method(const ros::Time &begin, const ros::Time &end, ros::Time &target)
    {
        auto begin_el   = std::find_if(data.begin(),data.end(),[&begin](std::pair<ros::Time, Eigen::Quaterniond> x) { return begin < x.first; });
        auto target_el  = std::find_if(data.begin(),data.end(),[&target](std::pair<ros::Time, Eigen::Quaterniond> x) { return target < x.first; });
        auto end_el     = std::find_if(data.begin(),data.end(),[&end](std::pair<ros::Time, Eigen::Quaterniond> x) { return end < x.first; });
        
        ros::Time standard_time = target_el->first;
        int num = std::distance(begin_el,end_el);
        Eigen::Quaterniond origin = target_el->second;

        

        Eigen::VectorXd time(num);
        Eigen::VectorXd angle_x(num);
        Eigen::VectorXd angle_y(num);
        Eigen::VectorXd angle_z(num);

        

        for(int i=0;i < num;++i)
        {
            time(i)  = ((begin_el+i)->first - standard_time).toSec();
            Eigen::VectorXd vec = Quaternion2Vector(((begin_el+i)->second * origin.conjugate()).normalized());
            // Eigen::VectorXd vec = Quaternion2Vector(origin.conjugate() * ((begin_el+i)->second).normalized());
            angle_x(i) = vec[0];
            angle_y(i) = vec[1];
            angle_z(i) = vec[2];
        }
        Eigen::VectorXd coeff_x = least_squares_method(time,angle_x,2);
        Eigen::VectorXd coeff_y = least_squares_method(time,angle_y,2);
        Eigen::VectorXd coeff_z = least_squares_method(time,angle_z,2);
        // printf("Coeff:%f %f %f\r\n",coeff(0),coeff(1),coeff(2));

        
        Eigen::Quaterniond lsm_value;
            double diff_t = (target - standard_time).toSec();
            lsm_value = Vector2Quaternion<double>(Eigen::Vector3d(  coeff_x(0) + diff_t * coeff_x(1) + pow(diff_t,2.0) * coeff_x(2),
                                                                    coeff_y(0) + diff_t * coeff_y(1) + pow(diff_t,2.0) * coeff_y(2),
                                                                    coeff_z(0) + diff_t * coeff_z(1) + pow(diff_t,2.0) * coeff_z(2))) * origin;
        
        {
                // For debug
                // angle_x = Eigen::VectorXd::Zero(500);
                //  angle_y = Eigen::VectorXd::Ones(500);
                // angle_z = -1*Eigen::VectorXd::Ones(500);
                // for(int i=0;i<angle_x.size();++i)
                // {
                //     angle_x(i) = i;
                //     angle_y(i) = i;
                //     angle_z(i) = i;
                // }
            
            // cv::Mat plot_data = cv::Mat::zeros(angle_x.size(),1,CV_64F);
            // // memcpy(plot_data.data,angle_x.data(),sizeof(double)*angle_x.size());
            int width = 640;
            int height = 480;
            cv::Mat plot_result_x = cv::Mat::zeros(height,width,CV_8UC3);//,plot_result_y,plot_result_z;
            for(int i=0;i<width;++i)
            {
                double gain = 500;
                int index = (double)i/width*(angle_x.size()-1);
                double value = angle_x(index) *gain + height/2;
                int r = std::min(height-1,std::max(0,(int)value));
                // printf("angle_x:%f\r\n",value);
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(0,0,255);


                value = angle_y(index) *gain + height/2;
                r = std::min(height-1,std::max(0,(int)value));
                // printf("angle_x:%f\r\n",value);
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(0,255,0);

                value = angle_z(index) *gain + height/2;
                r = std::min(height-1,std::max(0,(int)value));
                // printf("angle_x:%f\r\n",value);
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(255,0,0);

                Eigen::Vector3d vec = Eigen::Vector3d(  coeff_x(0) + time(index) * coeff_x(1) + pow(time(index),2.0) * coeff_x(2),
                                                                    coeff_y(0) + time(index) * coeff_y(1) + pow(time(index),2.0) * coeff_y(2),
                                                                    coeff_z(0) + time(index) * coeff_z(1) + pow(time(index),2.0) * coeff_z(2));

                value = vec[0] *gain + height/2;
                r = std::min(height-1,std::max(0,(int)value));
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(127,127,255);

                value = vec[1] *gain + height/2;
                r = std::min(height-1,std::max(0,(int)value));
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(127,255,127);

                value = vec[2] *gain + height/2;
                r = std::min(height-1,std::max(0,(int)value));
                plot_result_x.at<cv::Vec3b>(r,i) = cv::Vec3b(255,127,127);
            }

            // cv::Ptr<cv::plot::Plot2d> plot = cv::plot::Plot2d::create(plot_data);
            // if(0)
            // {
            //     //  plot = cv::plot::Plot2d::create(plot_data);
            //     memcpy(plot_data.data,angle_x.data(),sizeof(double)*angle_x.size());
            //     plot_data.at<double>(0,0) = 2.0;
            //     plot_data.at<double>(1,0) = -2.0;
                
            //      plot->setPlotBackgroundColor(cv::Scalar(0,0,0));
            //     plot->setPlotLineColor(cv::Scalar(0,0,255));
            //     plot->render(plot_result_x);
            // }
        
            // plot->setPlotBackgroundColor(cv::Scalar(0,0,0));
            // plot->setPlotLineColor(cv::Scalar(0,0,255));
            // memcpy(plot_data.data,angle_x.data(),sizeof(double)*angle_x.size());
            // plot->render(plot_result_x);

            // plot->setPlotLineColor(cv::Scalar(0,255,0));
            // memcpy(plot_data.data,angle_y.data(),sizeof(double)*angle_y.size());
            // plot->render(plot_result_y);

            // plot->setPlotLineColor(cv::Scalar(255,0,0));
            // memcpy(plot_data.data,angle_z.data(),sizeof(double)*angle_z.size());
            // plot->render(plot_result_z);

            // cv::imshow("plot",plot_result_x+plot_result_y+plot_result_z);
                cv::imshow("plot",plot_result_x);
            cv::waitKey(1);

        }


        return lsm_value.normalized();
    }

} // namespace virtualgimbal