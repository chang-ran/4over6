package com.java.a4over6;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.VpnService;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

public class MainActivity extends AppCompatActivity {

    private Button connect ;
    private Button disconnect;
    private EditText address;
    private EditText port;
    private TextView flow_view;
    private boolean running;
    private Timer timer_flow;
    private Timer timer_set;
    private TimerTask task_flow;
    private TimerTask set_flow;
    Handler handler = new Handler(Looper.getMainLooper());
    private String[] dns;
    private boolean ip_flag;
    private String ip_addr;
    private  int sockfd;
    private  int tunfd;
    private  String route;
    private String flow_text;
    Receiver vpnreceiver;
    String[] permissions = new String[]{
            Manifest.permission.INTERNET,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.BIND_VPN_SERVICE
    };

    public class Receiver extends BroadcastReceiver{
        @Override
        public void onReceive(Context context, Intent intent) {

            tunfd = intent.getIntExtra("tunfd",-1);
            setTunfd(tunfd);
            Log.d("frontend", "tunfd: " + String.valueOf(tunfd) + ", just been sent");
//            start();
        }
    }

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);
        dns = new String[3];
        running = false;
        ip_flag = false;
        flow_text = "not in connection";
        flow_view = findViewById(R.id.flow);
        new Thread(){
            @Override
            public void run() {
                ip_task();
            }
        }.start();

        task_init();
        timer_flow.schedule(task_flow, 0, 1000);
        timer_set.schedule(set_flow,0,1000);

//
//
        vpnreceiver = new Receiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction("casttun");
        registerReceiver(vpnreceiver, filter);
        view_init();

    }
    protected void  task_init(){
        timer_flow = new Timer();
        timer_set = new Timer();
        set_flow = new TimerTask() {
            @Override
            public void run() {
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        flow_view.setText(flow_text);
                    }
                });
            }
        };
        task_flow = new TimerTask() {
            @Override
            public void run() {
                if (running){
                    final String ans = getflow();
                    String[] infos = ans.split(" ");
                    int time = Integer.parseInt(infos[4]);
                    flow_text ="IPv4: "+ ip_addr +
                            "\nsend speed: " + getFlow(String.valueOf(Double.parseDouble(infos[0])/time)) + "/s;   receive speed: " +
                            getFlow(String.valueOf(Double.parseDouble(infos[2])/time)) + "/s\ntotal send: " + getFlow(infos[0]) + ";   send count: " +
                            infos[1] + "\ntotal receive: " +  getFlow(infos[2])+ ";    receive count : " +  infos[3]
                            + "\ntime: " +  getTime(infos[4]);
                }else{
                    flow_text = "not in connection";

                }
//                Log.d("frontend", flow_text);
            }
        };
//        task_ip = new TimerTask() {
//            @Override
//            public void run() {
//                if (!ip_flag) {
//                    String ip_info = getip();
//                    if (ip_info.length() > 10) {
//                        ip_flag = true;
//                        String[] ip_infos = ip_info.split(" ");
//                        int cursor = 0;
//                        sockfd = Integer.parseInt(ip_infos[cursor++]);
//                        ip_addr = ip_infos[cursor++];
//                        route = ip_infos[cursor++];
//                        dns[0] = ip_infos[cursor++];
//                        dns[1] = ip_infos[cursor++];
//                        dns[2] = ip_infos[cursor];
//                        Log.d("frontend", "ip_addr: " + ip_addr);
//                        Log.d("frontend", "route: " + route);
//                        Log.d("frontend", "dns[0]: " + dns[0]);
//                        Log.d("frontend", "dns[1]: " + dns[1]);
//                        Log.d("frontend", "dns[2]" + dns[2]);
//                        // open VPNService
//                        startVpnService();
//                    }
//                }else{
//                    cancel();
//                }
//            }
//        };
    }
    void ip_task(){
        while (!ip_flag) {
            String ip_info = getip();
            if (ip_info.length() > 10) {
                ip_flag = true;
                String[] ip_infos = ip_info.split(" ");
                int cursor = 0;
                int len = ip_infos.length;
                sockfd = Integer.parseInt(ip_infos[cursor++]);
                ip_addr = ip_infos[cursor++];
                route = ip_infos[cursor++];
                dns[0] = ip_infos[cursor++];
                if (cursor<=len-1) dns[1] = ip_infos[cursor++];
                if (cursor<=len-1) dns[2] = ip_infos[cursor];
                Log.d("frontend", "ip_addr: " + ip_addr);
                Log.d("frontend", "route: " + route);
                Log.d("frontend", "dns[0]: " + dns[0]);
                Log.d("frontend", "dns[1]: " + dns[1]);
                Log.d("frontend", "dns[2]" + dns[2]);
                // open VPNService
                startVpnService();
            }
        }
    }
    protected void view_init(){
        connect = findViewById(R.id.connect);
        disconnect = findViewById(R.id.disconnect);
        address = findViewById(R.id.address);
        port = findViewById(R.id.port);
        connect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (!running){
                    running = true;
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            buildconnection(address.getText().toString(), port.getText().toString());
                        }
                    }).start();
                }else{
                    Toast.makeText(MainActivity.this, "后台正在运行",Toast.LENGTH_SHORT).show();
                }

            }
        });
        disconnect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                running = false;
                breakconnection();

            }
        });
    }
    private void startVpnService() {
        Intent intent = VpnService.prepare(this);
        if (intent != null ){
            startActivityForResult(intent, 0);
        }else{
            onActivityResult(0 ,RESULT_OK, null);
        }
    }

    protected String getFlow(String s) {
        if (s.isEmpty()) return "0B";
        double d = Double.parseDouble(s);
        int cnt = 0;
        while (d > 1024) {
            d = d / 1024;
            cnt++;
        }
        String format = String.format("%.2f", d);
        if (cnt == 0) return format + "B";
        else if (cnt == 1) return format + "KB";
        else if (cnt == 2) return format + "MB";
        else return format + "GB";
    }
    protected String getTime(String s) {
        try {
            Integer wholeTime = Integer.parseInt(s);
            Integer hour, min, sec;
            sec = wholeTime % 60;
            hour = wholeTime / 3600;
            min = (wholeTime / 60) % 60;
            return hour.toString() + ":" + min.toString() + ":" + sec.toString();
        } catch (Exception e) {
            Log.d("frontend", e.toString());
            return "0:0:0";
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Intent intent = new Intent(this,MyVpnService.class);

        intent.putExtra("session", "test from main");
        intent.putExtra("socket", sockfd);

        intent.putExtra("ip_addr", ip_addr);
        intent.putExtra("route", route);
        intent.putExtra("dns0", dns[0]);
        intent.putExtra("dns1", dns[1]);
        intent.putExtra("dns2", dns[2]);

        startService(intent);

    }
    void start(){
                new Thread(){
            @Override
            public void run() {
                listen();
            }
        }.start();
        new Thread(){
            @Override
            public void run() {
                readtun();
            }
        }.start();
        new Thread(){
            @Override
            public void run() {
                heartbeat();
            }
        }.start();
    }
    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native void buildconnection(String addr, String port);
    public native void breakconnection();
    public native String getip();
    public native String getflow();
    public native String setTunfd(int tun);
    public native void listen();
    public native void readtun();
    public native void heartbeat();

    protected void onDestroy() {
        unregisterReceiver(vpnreceiver);
        super.onDestroy();
    }
}
