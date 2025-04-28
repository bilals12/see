const DEBUG = true;  // Set to false to disable debug logging

function debugLog(message, data) {
    if (DEBUG) {
        console.log(`[DEBUG] ${message}`, data ? data : '');
    }
}

// define colors for each event
const eventColors = {
    leftclicks: '#90EE90',    // Light green
    rightclicks: '#98FB98',   // Pale green
    middleclicks: '#98FF98',  // Mint green
    keypresses: '#7FFF00',    // Chartreuse
    mousemoves: '#00FF7F'     // Spring green
};

// Fix cumulative data display
fetch('/viz/cumulative_data.csv')
    .then(response => response.text())
    .then(data => {
        debugLog('Cumulative data:', data);
        const lines = data.split('\n');
        if (lines.length > 1) {
            const [keypresses, mousemoves, leftclicks, rightclicks] = lines[1].split(',');
            
            document.getElementById('keypresses').textContent = keypresses;
            document.getElementById('mousemoves').textContent = mousemoves + 'm';
            document.getElementById('leftclicks').textContent = leftclicks;
            document.getElementById('rightclicks').textContent = rightclicks;
        }
    })
    .catch(error => console.error('error loading cumulative data:', error));

// fetch past 24h data + generate chart
fetch('/viz/past_24_hours_data.csv')
    .then(response => response.text())
    .then(data => {
        const parsedData = parseCSV(data);
        renderChart(parsedData);
    })
    .catch(error => console.error('error loading past 24 hours data:', error));

function parseCSV(data) {
    debugLog('Raw CSV data:', data);
    const lines = data.split('\n')
        .filter(line => line.trim() && !line.startsWith('cumulative')); // Skip header and cumulative line
    
    debugLog('Filtered lines:', lines);
    
    const parsedData = lines.map(line => {
        const [timestamp, keypresses, mousemoves, leftclicks, rightclicks, middleclicks] = line.split(',');
        const parsed = {
            timestamp: new Date(parseInt(timestamp) * 1000),
            keypresses: parseInt(keypresses) || 0,
            mousemoves: parseFloat(mousemoves.replace(' meters', '')) || 0,
            leftclicks: parseInt(leftclicks) || 0,
            rightclicks: parseInt(rightclicks) || 0,
            middleclicks: parseInt(middleclicks) || 0
        };
        debugLog('Parsed line:', parsed);
        return parsed;
    }).filter(d => !isNaN(d.timestamp.getTime()));

    debugLog('Final parsed data:', parsedData);
    return parsedData;
}

function renderChart(data) {
    if (!data || data.length === 0) {
        debugLog('No data to render');
        return;
    }
    debugLog('Rendering chart with data points:', data.length);
    
    // Clear existing chart and tooltips
    d3.select('#chart').html('');
    d3.selectAll('.tooltip').remove();

    // Create tooltip
    const tooltip = d3.select('body')
        .append('div')
        .attr('class', 'tooltip')
        .style('position', 'absolute')
        .style('opacity', 0)
        .style('background', 'rgba(0, 255, 0, 0.1)')
        .style('border', '1px solid #0f0')
        .style('color', '#0f0')
        .style('padding', '8px')
        .style('border-radius', '4px')
        .style('pointer-events', 'none')
        .style('font-family', 'monospace')
        .style('z-index', '100');

    // Make chart responsive to container size
    const chartContainer = d3.select('#chart');
    const containerWidth = parseInt(chartContainer.style('width'));
    const containerHeight = parseInt(chartContainer.style('height'));
    
    const margin = {top: 20, right: 20, bottom: 30, left: 50};
    const width = (containerWidth || 800) - margin.left - margin.right;
    const height = (containerHeight || 400) - margin.top - margin.bottom;

    // Create SVG with full container size
    const svg = chartContainer
        .append('svg')
        .style('width', '100%')
        .style('height', '100%')
        .attr('viewBox', `0 0 ${width + margin.left + margin.right} ${height + margin.top + margin.bottom}`)
        .append('g')
        .attr('transform', `translate(${margin.left},${margin.top})`);

    // Define scales with 24h time format
    const x = d3.scaleTime()
        .domain(d3.extent(data, d => d.timestamp))
        .range([0, width]);

    const xAxis = d3.axisBottom(x)
        .tickFormat(d => {
            const hours = d.getHours();
            const minutes = d.getMinutes();
            const ampm = hours >= 12 ? 'PM' : 'AM';
            const hour12 = hours % 12 || 12;
            // Remove padding from hours, keep padding for minutes
            return `${hour12}:${minutes.toString().padStart(2, '0')} ${ampm}`;
        })
        .ticks(d3.timeMinute.every(15))
        .tickSizeOuter(0);

    const y = d3.scaleLinear()
        .domain([0, d3.max(data, d => 
            Math.max(
                d.keypresses, 
                d.mousemoves, 
                d.leftclicks, 
                d.rightclicks,
                d.middleclicks
            )
        ) * 1.1]) // Add 10% padding to top
        .range([height, 0]);

    // Update grid lines to be more frequent but lighter
    svg.append('g')
        .attr('class', 'grid')
        .attr('transform', `translate(0,${height})`)
        .call(d3.axisBottom(x)
            .ticks(d3.timeMinute.every(5))  // More frequent grid lines
            .tickSize(-height)
            .tickFormat('')
        )
        .style('stroke', '#0f0')
        .style('stroke-opacity', 0.05);  // Make grid lines lighter

    svg.append('g')
        .attr('class', 'grid')
        .call(d3.axisLeft(y)
            .tickSize(-width)
            .tickFormat('')
        )
        .style('stroke', '#0f0')
        .style('stroke-opacity', 0.1);

    // Style axes
    svg.append('g')
        .attr('transform', `translate(0,${height})`)
        .call(d3.axisBottom(x))
        .style('color', '#0f0')
        .style('font-family', 'monospace')
        .style('font-size', '12px');

    svg.append('g')
        .call(d3.axisLeft(y))
        .style('color', '#0f0');

    // Remove middleclicks from event colors
    const filteredEventColors = Object.fromEntries(
        Object.entries(eventColors).filter(([key]) => key !== 'middleclicks')
    );

    // Update tooltip format function
    const tooltipFormat = (metric, value) => {
        const formattedValue = metric === 'mousemoves' ? 
            `${value.toFixed(2)}m` : 
            value.toString();
        
        const metricDisplay = {
            leftclicks: 'Left Clicks',
            rightclicks: 'Right Clicks',
            keypresses: 'Keypresses',
            mousemoves: 'Mouse Movement'
        };
        
        return `${metricDisplay[metric]}: ${formattedValue}`;
    };

    // Add tooltip positioning logic
    function getTooltipPosition(event, chartContainer) {
        const tooltipWidth = 150;  // Approximate width of tooltip
        const tooltipHeight = 40;  // Approximate height of tooltip
        const padding = 10;  // Padding from edges
        
        // Get container boundaries
        const containerRect = chartContainer.node().getBoundingClientRect();
        const rightEdge = containerRect.right;
        const bottomEdge = containerRect.bottom;
        
        // Calculate position
        let xPos = event.pageX + padding;
        let yPos = event.pageY - tooltipHeight - padding;
        
        // Adjust if too close to right edge
        if (xPos + tooltipWidth > rightEdge) {
            xPos = event.pageX - tooltipWidth - padding;
        }
        
        // Adjust if too close to bottom edge
        if (yPos + tooltipHeight > bottomEdge) {
            yPos = event.pageY - tooltipHeight - padding;
        }
        
        return { x: xPos, y: yPos };
    }

    // Update points with simpler tooltip
    Object.entries(filteredEventColors).forEach(([metric, color]) => {
        debugLog(`Adding line for ${metric}`);
        
        const line = d3.line()
            .x(d => x(d.timestamp))
            .y(d => y(d[metric]))
            .defined(d => !isNaN(d[metric])); // Skip missing values

        // Add the line
        svg.append('path')
            .datum(data)
            .attr('fill', 'none')
            .attr('stroke', color)
            .attr('stroke-width', 2)
            .attr('d', line);

        svg.selectAll(`dot-${metric}`)
            .data(data)
            .enter()
            .append('circle')
            .attr('cx', d => x(d.timestamp))
            .attr('cy', d => y(d[metric]))
            .attr('r', 3)
            .attr('fill', color)
            .attr('opacity', 0.7)
            .on('mouseover', (event, d) => {
                const pos = getTooltipPosition(event, chartContainer);
                tooltip.transition()
                    .duration(200)
                    .style('opacity', .9);
                tooltip.html(tooltipFormat(metric, d[metric]))
                    .style('left', pos.x + 'px')
                    .style('top', pos.y + 'px');
            })
            .on('mouseout', () => {
                tooltip.transition()
                    .duration(500)
                    .style('opacity', 0);
            });
    });

    

    // Remove legend at bottom
    d3.select('.legend').remove();

    // Add auto-refresh every minute
    setInterval(() => {
        fetch('/viz/cumulative_data.csv')
            .then(response => response.text())
            .then(data => {
                const lines = data.split('\n');
                if (lines.length > 1) {
                    const [keypresses, mousemoves, leftclicks, rightclicks] = lines[1].split(',');
                    document.getElementById('keypresses').textContent = keypresses;
                    document.getElementById('mousemoves').textContent = mousemoves + 'm';
                    document.getElementById('leftclicks').textContent = leftclicks;
                    document.getElementById('rightclicks').textContent = rightclicks;
                }
            });
    }, 60000);
}

