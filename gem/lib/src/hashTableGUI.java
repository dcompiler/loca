
import java.awt.BorderLayout;

import java.awt.Dimension;

import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class hashTableGUI implements ActionListener, WindowListener{
	
	private JButton okButton;
	
	private JTextField[] tfArray;
	
	private JLabel[] labArray;
    
	private CountDownLatch latch;
	
	private JFrame f;
	
	public boolean verify(ArrayList<myEntry<String, String>> table){
		String message = "Here is the data: \n";
		for(myEntry<String, String> e: table){
			message += (e.getKey()+ ": " +e.getValue()+ "\n");
		}
	        String[] choices = {"OK", "MODIFY"};
			int c = JOptionPane.showOptionDialog(null, message,
					"Results", JOptionPane.YES_NO_OPTION, JOptionPane.PLAIN_MESSAGE, 
					null, choices, "");
			if(c == 0){
			   return true;
			}
			return false;
	}
	
	public ArrayList<myEntry<String, String>> getInputs(ArrayList<myEntry<String, String>> table){
		instantiateGUI(table);
        latch = new CountDownLatch(1);
        try {
			latch.await();
		} catch (InterruptedException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}
		int idx = 0;
        for(myEntry<String, String> e: table){
          	e.setValue(tfArray[idx].getText());
          	idx++;
        }
        f.dispose();
		return table;
	}
	
	public void instantiateGUI(ArrayList<myEntry<String, String>> table){
		f = new JFrame();
		
		tfArray = new JTextField[table.size()];
		labArray = new JLabel[table.size()];
		
		int next = 0;
		for(myEntry<String, String> e: table){
			JTextField tf = new JTextField(e.getValue());
			JLabel label = new JLabel(e.getKey());
			tfArray[next] = tf;
			labArray[next] = label;
			next++;
		}
		
		okButton = new JButton("OK");
		okButton.addActionListener(this);
		
		JPanel p1 = new JPanel();
		p1.setLayout(new BoxLayout(p1, BoxLayout.Y_AXIS));
		
		for(int i = 0; i < table.size(); i++){
		  JPanel panel = new JPanel();
		  panel.setLayout(new BoxLayout(panel, BoxLayout.X_AXIS));
		  panel.add(labArray[i]);
		  panel.add(tfArray[i]);
		  p1.add(panel);
		}
		f.add(p1, BorderLayout.CENTER);
		f.add(okButton, BorderLayout.SOUTH);
		
		f.addWindowListener(this);
		
		f.setTitle("Modify Screen");
		f.setLocationRelativeTo(null);
		f.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
		f.setVisible(true);
		f.pack();
	}
	
	public static void main(String[] args){
		/*  */
		ArrayList<myEntry<String, String>> table = new ArrayList<myEntry<String, String>>();
		table.add(new myEntry<String, String>("First Name", "Alex simon"));
		table.add(new myEntry<String, String>("Last Name", ""));
		table.add(new myEntry<String, String>("Address", "ssh://p/compiler/hg-repos/loclab/iloveprogramming/doggy"));
		table.add(new myEntry<String, String>("Fav. Color", "Orange"));
		table.add(new myEntry<String, String>("Phone Number", "?"));
		
		hashTableGUI h = new hashTableGUI();
		table = h.getInputs(table);
		table = h.getInputs(table);
		h.verify(table);
		table = h.getInputs(table);
		System.out.println(table.get(0).getValue());
	}

	@Override
	public void actionPerformed(ActionEvent e) {
		if((e.getSource() == okButton) || (e.getSource() == f)){
			f.dispose();
		}
	}

	@Override
	public void windowActivated(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void windowClosed(WindowEvent e) {
		latch.countDown();
	}

	@Override
	public void windowClosing(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void windowDeactivated(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void windowDeiconified(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void windowIconified(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void windowOpened(WindowEvent e) {
		// TODO Auto-generated method stub
		
	}
}
