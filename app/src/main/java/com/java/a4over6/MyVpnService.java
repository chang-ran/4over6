package com.java.a4over6;

import android.content.Intent;
import android.net.VpnService;
import android.os.ParcelFileDescriptor;
import android.util.Log;

public class MyVpnService extends VpnService {
    private ParcelFileDescriptor mInterface;
    Builder builder = new Builder();
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        int sockfd = intent.getIntExtra("socket",-1);
        protect(sockfd);
        mInterface = builder.setSession(intent.getStringExtra("session"))
                .addAddress(intent.getStringExtra("ip_addr"), 24)
                .addRoute(intent.getStringExtra("route"), 0)
                .addDnsServer(intent.getStringExtra("dns0"))
                .addDnsServer(intent.getStringExtra("dns1"))
                .addDnsServer(intent.getStringExtra("dns2"))
                .setMtu(1500)
                .establish();
        int tunfd = mInterface.getFd();
        Intent cast = new Intent();
        cast.putExtra("tunfd",tunfd);
        cast.setAction("casttun");
        sendBroadcast(cast);
        Log.d("vpn", "Send Broadcast");
        return START_STICKY;
    }

}
